// ────────────────────────────────────────────
//  File: event_listener.hpp · Created by Yash Patel · 9-26-2025
// ────────────────────────────────────────────

#pragma once
#include <cstdint>

namespace keplar
{
    class EventListener
    {
        public:
            virtual ~EventListener() = default;

            // window events
            virtual void onWindowResize(uint32_t, uint32_t) {}
            virtual void onWindowClose() {}
            virtual void onWindowFocus(bool) {}

            // keyboard events
            virtual void onKeyPressed(uint32_t) {}
            virtual void onKeyReleased(uint32_t) {}

            // mouse events
            virtual void onMouseMove(double, double) {}
            virtual void onMouseScroll(double) {}
            virtual void onMouseButtonPressed(uint32_t, int, int) {}
            virtual void onMouseButtonReleased(uint32_t, int, int) {}
    };
}   // namespace keplar
