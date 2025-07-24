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
            Win32Platform() noexcept;
            virtual ~Win32Platform() override;

            virtual bool initialize(const std::string& title, int width, int height) noexcept override;
            virtual void pollEvents() noexcept override;
            virtual bool shouldClose() noexcept override;
            virtual void shutdown() noexcept override;

            virtual VkSurfaceKHR createVulkanSurface(VkInstance vkInstance) const noexcept override;
            virtual std::vector<std::string_view> getSurfaceInstanceExtensions() const noexcept override;
            virtual void* getNativeWindowHandle() const noexcept override;
            virtual uint32_t getWindowWidth() const noexcept override;
            virtual uint32_t getWindowHeight() const noexcept override;

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
