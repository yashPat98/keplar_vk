// ────────────────────────────────────────────
//  File: win32_platform.hpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#pragma once

#include <Windows.h>
#include "platform.hpp"
#include "event_manager.hpp"

namespace keplar
{
    class Win32Platform : public Platform
    {
        public:
            Win32Platform() noexcept;
            virtual ~Win32Platform() override;

            // lifecycle
            virtual bool initialize(const std::string& title, int width, int height) noexcept override;
            virtual void pollEvents() noexcept override;
            virtual bool shouldClose() noexcept override;
            virtual void shutdown() noexcept override;

            // window queries
            virtual void* getWindowHandle() const noexcept override;
            virtual uint32_t getWindowWidth() const noexcept override;
            virtual uint32_t getWindowHeight() const noexcept override;

            // event listeners
            virtual void addListener(EventListener* listener) noexcept override;
            virtual void removeListener(EventListener* listener) noexcept override;

            // vulkan 
            virtual VkSurfaceKHR createSurface(VkInstance vkInstance) const noexcept override;
            virtual std::vector<std::string_view> getSurfaceExtensions() const noexcept override;

        private:
            void toggleFullscreen() noexcept;
            static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
            
        private:
            // window handles and state
            HINSTANCE           m_hInstance;
            HWND                m_hwnd;
            WINDOWPLACEMENT     m_windowPlacement;
            DWORD               m_windowStyle;
            uint32_t            m_width;
            uint32_t            m_height;
            bool                m_shouldClose;
            bool                m_isFullscreen;
            EventManager        m_eventManager;
    };
}   // namespace keplar
