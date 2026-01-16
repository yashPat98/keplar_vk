// ────────────────────────────────────────────
//  File: time.hpp · Created by Yash Patel · 1-16-2026
// ────────────────────────────────────────────

#pragma once

#include <chrono>

namespace keplar
{
    class Time 
    {
        public:
            // creation and destruction
            Time() noexcept;

            // usage
            float tick() noexcept;

        private:
            using clock = std::chrono::steady_clock;
            using time_point = clock::time_point;

            bool        m_initialized;
            float       m_deltaTime;
            time_point  m_lastTime;
    };
}   // namespace keplar


