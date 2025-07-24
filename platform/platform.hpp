// ────────────────────────────────────────────
//  File: platform.hpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#pragma once

#include <string>
#include <vector>
#include "vulkan/vk_config.hpp"

namespace keplar
{
    class Platform
    {
        public:
            virtual ~Platform() = default;

            virtual bool initialize(const std::string& title, int width, int height) noexcept = 0;
            virtual void pollEvents() noexcept = 0;
            virtual bool shouldClose() noexcept = 0;
            virtual void shutdown() noexcept = 0;
            
            virtual VkSurfaceKHR createVulkanSurface(VkInstance vkInstance) const noexcept = 0;
            virtual std::vector<std::string_view> getSurfaceInstanceExtensions() const noexcept = 0;
            virtual void* getNativeWindowHandle() const noexcept = 0;
            virtual uint32_t getWindowWidth() const noexcept = 0;
            virtual uint32_t getWindowHeight() const noexcept = 0;
    };
}   // namespace keplar
