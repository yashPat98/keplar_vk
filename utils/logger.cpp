// ────────────────────────────────────────────
//  File: logger.cpp · Created by Yash Patel · 6-22-2025
// ────────────────────────────────────────────

#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip>

namespace keplar
{
    const std::string kDefaultLogFile = "vklog.txt";

    Logger& Logger::getInstance() noexcept 
    {
        static Logger instance(kDefaultLogFile);
        return instance;
    }

    Logger::Logger(const std::string& filename) noexcept
        : m_filename(filename)
        , m_logStream(m_filename, std::ios::out | std::ios::trunc)
        , m_enabledLevels{ Level::Trace, Level::Debug, Level::Info, Level::Warn, Level::Error, Level::Fatal }
        , m_shutdown(false)
        , m_worker(std::thread(&Logger::processQueue, this))
    {
        if (!m_logStream.is_open()) 
        {
            std::cerr << "Logger: failed to open log file: " << m_filename << std::endl;
        }

        // disable Debug/Trace in release builds
        #ifndef NDEBUG
            m_enabledLevels = { Level::Trace, Level::Debug, Level::Info, Level::Warn, Level::Error, Level::Fatal };
        #else
            m_enabledLevels = { Level::Info, Level::Warn, Level::Error, Level::Fatal };
        #endif
    }

    Logger::~Logger()
    {
        terminate();
    }

    void Logger::enqueueLog(Level level, const char* file, int line, const char* fmt, ...) noexcept
    {
        if (!isEnabled(level)) 
        {
            return;
        }

        constexpr size_t stackSize = 1024;
        char stackBuffer[stackSize];

        va_list args;
        va_start(args, fmt);
        int len = std::vsnprintf(stackBuffer, stackSize, fmt, args);
        va_end(args);

        if (len < 0) 
        {
            return;
        }

        std::string timestamp = getCurrentTimestamp();
        std::string fileLine = getFileLine(file, line);

        std::string formatted;
        if (static_cast<size_t>(len) < stackSize)
        {
            formatLogMessage(formatted, level, timestamp.c_str(), fileLine.c_str(), stackBuffer);
        }
        else
        {
            // fall back to heap buffer for large messages
            std::vector<char> heapBuffer(len + 1);
            va_list args2;
            va_start(args2, fmt);
            std::vsnprintf(heapBuffer.data(), heapBuffer.size(), fmt, args2);
            va_end(args2);
            formatLogMessage(formatted, level, timestamp.c_str(), fileLine.c_str(), heapBuffer.data());
        }

        // equeue the formatted log message for async processing
        {
            std::lock_guard lock(m_queueMutex);
            m_messageQueue.push({ level, std::move(formatted) });
        }

        // notify the worker thread 
        m_cv.notify_one();
    }

    void Logger::flush() noexcept
    {
        std::scoped_lock lock(m_stateMutex, m_queueMutex);
    
        // now it's safe: no one can enqueue while we drain the queue
        while (!m_messageQueue.empty())
        {
            auto logEntry = std::move(m_messageQueue.front());
            m_messageQueue.pop();
            if (m_logStream.is_open())
            {
                m_logStream << logEntry.mMessage << "\n";
            }
        }

        // force flush stream
        if (m_logStream.is_open())
        {
            m_logStream.flush();
        }
    }

    void Logger::terminate() noexcept
    {
        // acquire both state and queue mutexes to safely update shutdown state
        {
            std::scoped_lock lock(m_stateMutex, m_queueMutex);
            if (m_shutdown.load()) return;
            m_shutdown.store(true);
        }

        // notify and wait for the worker thread 
        m_cv.notify_one();
        if (m_worker.joinable())
        {
            m_worker.join();
        }

        // flush any remaining log messages
        flush();

        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_logStream.is_open())
        {
            m_logStream.close();
        }
    }

    void Logger::restart(const std::string& filename) noexcept
    {
        terminate();
        
        std::scoped_lock lock(m_stateMutex, m_queueMutex);
        std::ios_base::openmode mode = std::ios::out;
        if (!filename.empty())
        {
            m_filename = filename;
            mode |= std::ios::trunc;
        }
        else 
        {
            mode |= std::ios::app;
        }

        m_logStream.open(m_filename, mode);
        if (!m_logStream.is_open()) 
        {
            std::cerr << "Logger: failed to open log file: " << m_filename << std::endl;
            return;
        }

        m_shutdown.store(false);
        m_worker = std::thread(&Logger::processQueue, this);
    }

    void Logger::resetLogFile(const std::string& filename) noexcept
    {
        if (filename.empty())
        {
            return;
        }

        std::scoped_lock lock(m_stateMutex, m_queueMutex);
        if (m_logStream.is_open())
        {
            m_logStream.close();
        }

        m_filename = filename;
        m_logStream.open(filename, std::ios::out | std::ios::trunc);
        if (!m_logStream.is_open()) 
        {
            std::cerr << "Logger: failed to open log file: " << m_filename << std::endl;
        }
    }

    bool Logger::isActive() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return (!m_shutdown.load() && m_logStream.is_open());
    }

    void Logger::setMinLevel(Level level) noexcept
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_enabledLevels.clear();
        for (int lvl = static_cast<int>(level); lvl <= static_cast<int>(Level::Fatal); ++lvl)
        {
            m_enabledLevels.insert(static_cast<Level>(lvl));
        }
    }

    void Logger::enableLevel(Level level) noexcept
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_enabledLevels.insert(level);
    }

    void Logger::disableLevel(Level level) noexcept
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_enabledLevels.erase(level);
    }

    bool Logger::isEnabled(Level level) const noexcept
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return (m_enabledLevels.find(level) != m_enabledLevels.end());
    }

    void Logger::processQueue() noexcept
    {
        std::unique_lock<std::mutex> queueLock(m_queueMutex);
        while (true)
        {
            // wait until there is a message to process or a shutdown is requested
            m_cv.wait(queueLock, [this] { return !m_messageQueue.empty() || m_shutdown.load(); });
            while (!m_messageQueue.empty())
            {
                // retrieve the next log entry from the queue
                auto logEntry = std::move(m_messageQueue.front());
                m_messageQueue.pop();

                // temporarily release the queue lock during I/O to allow other threads to enqueue
                queueLock.unlock();
                {
                    std::lock_guard<std::mutex> stateLock(m_stateMutex);
                    if (m_logStream.is_open())
                    {
                        m_logStream << logEntry.mMessage << "\n";
                        
                        // flush critical logs 
                        if (logEntry.mLevel == Level::Error || logEntry.mLevel == Level::Fatal)
                        {
                            m_logStream.flush();
                        }
                    }
                }
                // re-acquire the queue lock before checking the queue again
                queueLock.lock();
            }

            // exit the loop if shutdown has been requested
            if (m_shutdown.load())
            {
                break;
            }
        }
    }

    void Logger::formatLogMessage(std::string& out, Level level, const char* timestamp, const char* fileLine, const char* message) noexcept
    {
        // reserve approximate size upfront to reduce reallocations
        out.clear();
        out.reserve(128 + std::strlen(message) + std::strlen(fileLine) + std::strlen(timestamp));

        // convert thread id to string without ostringstream
        std::stringstream tidStream;
        tidStream << std::this_thread::get_id();
        std::string tidStr = tidStream.str();

        if (tidStr.length() < 6)
            tidStr.append(6 - tidStr.length(), ' ');

        // get level string
        std::string levelStr(levelToString(level));
        if (levelStr.length() < 5)
            levelStr.append(5 - levelStr.length(), ' ');

        // build the final string 
        out += timestamp;
        out += " | TID ";
        out += tidStr;
        out += " | ";
        out += levelStr;
        out += " | ";

        // file and line information
        size_t fileLineLen = std::strlen(fileLine);
        out.append(fileLine, fileLineLen);
        if (fileLineLen < 25)
            out.append(25 - fileLineLen, ' ');

        out += " | ";
        out += message;
    }

    const char* Logger::levelToString(Level level) const noexcept
    {
        switch (level) 
        {
            case Level::Trace: 
                return "Trace";
            case Level::Debug: 
                return "Debug";
            case Level::Info: 
                return "Info";
            case Level::Warn: 
                return "Warn";
            case Level::Error: 
                return "Error";
            case Level::Fatal: 
                return "Fatal";
            default: 
                return "Unknown";
        }
    }

    std::string Logger::getCurrentTimestamp() const noexcept
    {
        using namespace std::chrono;
        static std::string lastTimestamp;
        static time_t lastSecond = 0;

        auto now = system_clock::now();
        auto in_time_t = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        // cheaper on repeated calls within same second
        if (in_time_t != lastSecond)
        {
            std::tm timeInfo;
            #ifdef _WIN32
                localtime_s(&timeInfo, &in_time_t);
            #else
                localtime_r(&in_time_t, &timeInfo);
            #endif
            std::ostringstream oss;
            oss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
            lastTimestamp = oss.str();
            lastSecond = in_time_t;
        }

        std::ostringstream finalStream;
        finalStream << lastTimestamp << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return finalStream.str();
    }

    std::string Logger::getFileLine(const char* fullPath, int line) const noexcept
    {
        std::string_view path(fullPath);
        size_t lastSlash = path.find_last_of("/\\");
        std::string_view filename = (lastSlash != std::string_view::npos) ? path.substr(lastSlash + 1) : path;
        return std::string(filename) + ":" + std::to_string(line);
    }
}   // namespace keplar
