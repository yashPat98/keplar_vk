// ────────────────────────────────────────────
//  File: frame_pacer.cpp · Created by Yash Patel · 10-28-2025
// ────────────────────────────────────────────

#include "frame_pacer.hpp"
#include <thread>

namespace 
{
    constexpr float kMinFPS         = 1.0f;
    constexpr float kMaxFPS         = 1000.0f;

    constexpr auto kSpinThreshold   = std::chrono::microseconds(1000);
    constexpr auto kSleepPad        = std::chrono::microseconds(500);
}

namespace keplar
{
    FramePacer::FramePacer() noexcept
        : m_targetFps(0.0f)
        , m_frameCount(0)
        , m_enabled(false)
        , m_hasSchedule(false)
        , m_frameStep{}
        , m_nextTick{}
    {
    }

    void FramePacer::setTargetFps(float fps) noexcept
    {
        if (fps <= 0.0f) 
        {
            m_enabled = false;
            m_hasSchedule = false;
            m_targetFps = 0.0f;
            m_frameStep = duration{};
            return;
        }

        if (fps < kMinFPS) fps = kMinFPS;
        if (fps > kMaxFPS) fps = kMaxFPS;

        m_enabled = true;
        m_hasSchedule = false;
        m_targetFps = fps;

        const auto step = std::chrono::duration<double>(1.0f / fps);
        m_frameStep = std::chrono::duration_cast<duration>(step);
    }

    void FramePacer::wait() noexcept
    {
        if (!m_enabled)
        {
            return;
        }

        const auto now = clock::now();
        if (!m_hasSchedule)
        {
            // initialize frame pacing state
            m_nextTick = now + m_frameStep;
            m_hasSchedule = true;
            return;
        }

        // catch up if we are behind schedule
        if (now >= m_nextTick)
        {
            const auto lag = now - m_nextTick;
            const auto missed = lag / m_frameStep + 1;
            m_nextTick += missed * m_frameStep;
        }

        auto remaining = m_nextTick - clock::now();

        // coarse sleep first
        if (remaining > kSpinThreshold)
        {
            const auto coarse = remaining - kSpinThreshold;
            if (coarse > kSleepPad)
            {
                std::this_thread::sleep_for(coarse - kSleepPad);
            }
        }

        // fine wait (spin with yield)
        while (clock::now() < m_nextTick)
        {
            std::this_thread::yield();
        }

        // advance schedule for next frame
        m_nextTick += m_frameStep;
    }
}   // namespace keplar
