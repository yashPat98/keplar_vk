// ────────────────────────────────────────────
//  File: event_manager.hpp · Created by Yash Patel · 9-26-2025
// ────────────────────────────────────────────

#pragma once

#include <vector>
#include <mutex>
#include "event_listener.hpp"

namespace keplar
{
    class EventManager final
    {
        public:
            // creation and destruction
            EventManager() noexcept = default;
            ~EventManager() = default;

            // non-copyable
            EventManager(const EventManager&) = delete;
            EventManager& operator=(const EventManager&) = delete;

            // listener management
            void addListener(EventListener* listener) noexcept;
            void removeListener(EventListener* listener) noexcept;
            void removeAllListeners() noexcept;

            // window events
            void onWindowResize(uint32_t width, uint32_t height) const noexcept;
            void onWindowClose() const noexcept;
            void onWindowFocus(bool focused) const noexcept; 

            // keyboard events
            void onKeyPressed(uint32_t key) const noexcept;
            void onKeyReleased(uint32_t key) const noexcept;

            // mouse events
            void onMouseMove(double xpos, double ypos) const noexcept;
            void onMouseScroll(double yoffset) const noexcept;
            void onMouseButtonPressed(uint32_t button) const noexcept;
            void onMouseButtonReleased(uint32_t button) const noexcept;

        private:
            // returns a snapshot of the current listeners for safe, lock-free dispatch
            std::vector<EventListener*> getListenerSnapshot() const noexcept;

        private:    
            mutable std::mutex m_mutex;
            std::vector<EventListener*> m_listeners;
    };
}   // namespace keplar
