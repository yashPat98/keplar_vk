// ────────────────────────────────────────────
//  File: logger.hpp · Created by Yash Patel · 6-22-2025
// ────────────────────────────────────────────

#pragma once

#include <cstdarg>
#include <fstream>
#include <string>
#include <unordered_set>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

#define VK_LOG(level, fmt, ...) \
    do { \
        if (keplar::Logger::getInstance().isEnabled(keplar::Logger::Level::level)) \
        { \
            keplar::Logger::getInstance().enqueueLog( \
                keplar::Logger::Level::level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

// zero runtime overhead for debug and trace logs in release builds
#ifndef NDEBUG
    #define VK_LOG_TRACE(fmt, ...) VK_LOG(Trace, fmt, ##__VA_ARGS__)
    #define VK_LOG_DEBUG(fmt, ...) VK_LOG(Debug, fmt, ##__VA_ARGS__)
#else
    #define VK_LOG_TRACE(fmt, ...) ((void)0)
    #define VK_LOG_DEBUG(fmt, ...) ((void)0)
#endif

#define VK_LOG_INFO(fmt, ...)  VK_LOG(Info,  fmt, ##__VA_ARGS__)
#define VK_LOG_WARN(fmt, ...)  VK_LOG(Warn,  fmt, ##__VA_ARGS__)
#define VK_LOG_ERROR(fmt, ...) VK_LOG(Error, fmt, ##__VA_ARGS__)
#define VK_LOG_FATAL(fmt, ...) VK_LOG(Fatal, fmt, ##__VA_ARGS__)

namespace keplar
{
    class Logger
    {
        public:
            // severity levels
            enum class Level
            {
                Trace = 0,
                Debug,
                Info,
                Warn,
                Error,
                Fatal
            };

            // singleton creation
            static Logger& getInstance() noexcept;

            // disable copy and move semantics
            Logger(const Logger&) = delete;
            Logger(const Logger&&) = delete;
            Logger& operator=(const Logger&) = delete;
            Logger& operator=(const Logger&&) = delete;

            // logging operations
            void enqueueLog(Level level, const char* file, int line, const char* fmt, ...) noexcept;
            void flush() noexcept;
            void terminate() noexcept;
            void restart(const std::string& filename = {}) noexcept;
            
            // log level control
            void resetLogFile(const std::string& filename) noexcept;
            bool isActive() const noexcept;
            void setMinLevel(Level level) noexcept;
            void enableLevel(Level level) noexcept;
            void disableLevel(Level level) noexcept;
            bool isEnabled(Level level) const noexcept;
            
        private:
            Logger(const std::string& filename) noexcept;
            ~Logger();

            // internal helpers
            void processQueue() noexcept;
            void formatLogMessage(std::string& out, Level level, const char* timestamp, const char* fileLine, const char* message) noexcept;
            
            const char* levelToString(Level level) const noexcept;
            std::string getCurrentTimestamp() const noexcept;
            std::string getFileLine(const char* fullPath, int line) const noexcept;
            
        private:
            struct LogEntry
            {
                Level mLevel;
                std::string mMessage;
            };

            mutable std::mutex m_stateMutex;
            mutable std::mutex m_queueMutex;

            std::string m_filename;
            std::ofstream m_logStream;
            std::unordered_set<Level> m_enabledLevels;
            std::queue<LogEntry> m_messageQueue;
            std::condition_variable m_cv;
            std::atomic<bool> m_shutdown;
            std::thread m_worker;
    };
}   // namespace keplar
