// ────────────────────────────────────────────
//  File: frame_limiter.cpp · Created by Yash Patel · 10-28-2025
// ────────────────────────────────────────────

#include "frame_limiter.hpp"

#include <thread>
#include "core/keplar_config.hpp"

namespace 
{
    constexpr auto kSleepThreshold = std::chrono::milliseconds(3);  // minimum wait time to trigger sleep
    constexpr auto kSleepOffset    = std::chrono::milliseconds(2);  // remaining wait time after sleep
}

namespace keplar
{
    FrameLimiter::FrameLimiter() noexcept
        : m_frameRate(keplar::config::kDefaultFrameRate)
        , m_frameTime(1.0f / m_frameRate)
        , m_deltaTime(0.0f)
        , m_frameCount(0)
        , m_lastFrameTime(clock::now())
    {
    }

    void FrameLimiter::waitForNextFrame() noexcept
    {
        using namespace std::chrono;

        // skip wait for first frame
        if (m_frameCount == 0)
        {
            m_lastFrameTime = clock::now();
            m_deltaTime = m_frameTime;  
            m_frameCount++;
            return;  
        }

        // calculate next frame time 
        const auto curremtTime = clock::now();
        const auto nextFrameTime = m_lastFrameTime + duration_cast<clock::duration>(duration(m_frameTime));

        // wait until next frame time
        if (curremtTime < nextFrameTime)
        {
            const auto waitTime = nextFrameTime - curremtTime;

            // coarse wait
            if (waitTime > kSleepThreshold)
            {
                std::this_thread::sleep_for(waitTime - kSleepOffset);
            }

            // busy-wait with yeild for last ~1ms
            while (clock::now() < nextFrameTime)
            {
                std::this_thread::yield();
            }
        }

        // update delta and advance frame time
        m_deltaTime = duration(clock::now() - m_lastFrameTime).count();
        m_lastFrameTime = nextFrameTime;
        m_frameCount++;
    }

    void FrameLimiter::setFrameRate(float frameRate) noexcept
    {
        m_frameRate = frameRate;
        m_frameTime = 1.0f / frameRate;
    }
}   // namespace keplar
