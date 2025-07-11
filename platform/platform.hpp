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

            virtual bool initialize(const std::string& title, int width, int height) = 0;
            virtual void pollEvents() = 0;
            virtual bool shouldClose() = 0;
            virtual void shutdown() = 0;
            
            virtual VkSurfaceKHR createVulkanSurface(VkInstance vkInstance) const = 0;
            virtual std::vector<std::string_view> getSurfaceInstanceExtensions() const = 0;
            virtual void* getNativeWindowHandle() const = 0;
            virtual uint32_t getWindowWidth() const = 0;
            virtual uint32_t getWindowHeight() const = 0;
    };
}   // namespace keplar
