// ────────────────────────────────────────────
//  File: vulkan_surface.hpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#pragma once

#include "platform/platform.hpp"
#include "vulkan_instance.hpp"

namespace keplar
{
    class VulkanSurface final
    {
        public:    
            VulkanSurface();
            ~VulkanSurface();

            // avoid copy and move
            VulkanSurface(const VulkanSurface&) = delete;
            VulkanSurface& operator=(const VulkanSurface&) = delete;
            VulkanSurface(VulkanSurface&&) noexcept = delete;
            VulkanSurface& operator=(VulkanSurface&&) noexcept = delete;

            // manage lifetime
            bool initialize(const VulkanInstance& instance, const Platform& platform);
            void destroy();

            bool canQueueFamilyPresent(VkPhysicalDevice vkPhysicalDevice, uint32_t queueFamilyIndex) const; 

            VkSurfaceKHR get() const;
            bool isValid() const;

        private:
            VkSurfaceKHR m_vkSurfaceKHR;
            VkInstance m_vkInstance;
    };
}   // namespace keplar
