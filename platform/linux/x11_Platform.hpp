// ────────────────────────────────────────────
//  File: x11_Platform.hpp · Created by Yash Patel · 5-12-2026
// ────────────────────────────────────────────

#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "platform/platform.hpp"
#include "platform/event_manager.hpp"

namespace keplar
{
    class X11Platform : public Platform
    {
        public:
            // creation and destruction
            X11Platform() noexcept;
            virtual ~X11Platform() override;

            // lifecycle
            virtual bool initialize(const std::string& title, int width, int height, bool maximized) noexcept override;
            virtual void pollEvents() noexcept override;
            virtual bool shouldClose() noexcept override;
            virtual void shutdown() noexcept override;

            // window queries
            virtual void* getWindowHandle() const noexcept override;
            virtual uint32_t getWindowWidth() const noexcept override;
            virtual uint32_t getWindowHeight() const noexcept override;

            // event listeners
            virtual void addListener(const std::shared_ptr<EventListener>& listener) noexcept override;
            virtual void removeListener(const std::shared_ptr<EventListener>& listener) noexcept override;
            virtual void enableImGuiEvents(bool enabled) noexcept override;

            // vulkan
            virtual VkSurfaceKHR createSurface(VkInstance vkInstance) const noexcept override;
            virtual std::vector<std::string_view> getSurfaceExtensions() const noexcept override;

        private:
            void toggleFullscreen() noexcept;
            void handleEvent(XEvent& event) noexcept;
            uint32_t translateKeysym(KeySym keysym) const noexcept;

        private:
            // display and window handles
            Display*    m_display;
            Window      m_window;
            int         m_screen;

            // ICCCM / EWMH atoms
            Atom        m_wmDeleteWindow;       // WM_DELETE_WINDOW — intercept close button
            Atom        m_wmState;              // _NET_WM_STATE
            Atom        m_wmStateFullscreen;    // _NET_WM_STATE_FULLSCREEN

            // window dimensions and state
            uint32_t    m_width;
            uint32_t    m_height;
            bool        m_shouldClose;
            bool        m_isFullscreen;

            // input / event handling
            EventManager m_eventManager;
            bool         m_imguiEvents;
    };
}   // namespace keplar
