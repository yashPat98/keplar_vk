// ────────────────────────────────────────────
//  File: frame_pacer.hpp · Created by Yash Patel · 10-28-2025
// ────────────────────────────────────────────

#pragma once

#include <chrono>

namespace keplar
{
    class FramePacer
    {
        public: 
            // creation and destruction
            FramePacer() noexcept;

            // usage 
            void wait() noexcept;
            void setTargetFps(float fps) noexcept;

            // accessors
            float    getFrameRate() const noexcept  { return m_targetFps; }
            uint64_t getFrameCount() const noexcept { return m_frameCount; }

        private:
            using clock      = std::chrono::steady_clock;
            using time_point = clock::time_point;
            using duration   = clock::duration;

            float       m_targetFps;
            uint64_t    m_frameCount;
            bool        m_enabled;
            bool        m_hasSchedule;
            duration    m_frameStep;
            time_point  m_nextTick;
    };
}   // namespace keplar
