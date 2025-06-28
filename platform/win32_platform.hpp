// ────────────────────────────────────────────
//  File: win32_platform.hpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#pragma once

#include <Windows.h>
#include "platform.hpp"

namespace keplar
{
    class Win32Platform : public Platform
    {
        public:
            Win32Platform();
            virtual ~Win32Platform() override;

            virtual bool initialize(const std::string& title, int width, int height) override;
            virtual void pollEvents() override;
            virtual bool shouldClose() override;
            virtual void shutdown() override;

            virtual VkSurfaceKHR createVulkanSurface(VkInstance vkInstance) const override;
            virtual void* getNativeWindowHandle() const override;
            virtual uint32_t getWindowWidth() const override;
            virtual uint32_t getWindowHeight() const override;

        private:
            void toggleFullscreen();
            static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
            
        private:
            HINSTANCE m_hInstance;
            HWND m_hwnd;
            WINDOWPLACEMENT m_windowPlacement;
            DWORD m_windowStyle;
            uint32_t m_width;
            uint32_t m_height;
            bool m_shouldClose;
            bool m_isFullscreen;
    };
}   // namespace keplar
