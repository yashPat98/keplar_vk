// ────────────────────────────────────────────
//  File: frame_limiter.hpp · Created by Yash Patel · 10-28-2025
// ────────────────────────────────────────────

#pragma once

#include <chrono>

namespace keplar
{
    class FrameLimiter
    {
        public: 
            // creation and destruction
            FrameLimiter() noexcept;

            // usage 
            void waitForNextFrame() noexcept;
            void setFrameRate(float frameRate) noexcept;

            // accessors
            float    getDeltaTime() const noexcept  { return m_deltaTime; }
            float    getFrameRate() const noexcept  { return m_frameRate; }
            uint64_t getFrameCount() const noexcept { return m_frameCount; }

        private:
            using clock = std::chrono::steady_clock;
            using time_point = clock::time_point;
            using duration = std::chrono::duration<float>;

            float      m_frameRate;
            float      m_frameTime;
            float      m_deltaTime;
            uint64_t   m_frameCount;
            time_point m_lastFrameTime;
    };
}   // namespace keplar
