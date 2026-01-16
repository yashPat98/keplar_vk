// ────────────────────────────────────────────
//  File: time.cpp · Created by Yash Patel · 1-16-2026
// ────────────────────────────────────────────

#include "time.hpp"

namespace keplar
{
    Time::Time() noexcept
        : m_initialized(false)
        , m_deltaTime(0.0f)
        , m_lastTime{}
    {
    }

    float Time::tick() noexcept
    {
        // current monotonic time
        const auto now = clock::now();
        if (!m_initialized) 
        {
            // initialize time reference
            m_lastTime = now;
            m_initialized = true;
            m_deltaTime = 1.0f / 60.0f; 
            return m_deltaTime;
        }

        // dt since last frame
        m_deltaTime = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;

        // clamp to avoid extreme spikes
        if (m_deltaTime > 0.1f) m_deltaTime = 0.1f;
        if (m_deltaTime < 0.0f) m_deltaTime = 0.0f;

        return m_deltaTime;
    }
}   // namespace keplar

