// ────────────────────────────────────────────
//  File: vulkan_surface.hpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "platform/platform.hpp"
#include "vulkan_instance.hpp"

namespace keplar
{
    class VulkanSurface final
    {
        public:    
            // creation and destruction
            static std::unique_ptr<VulkanSurface> create(const VulkanInstance& instance, const Platform& platform) noexcept;
            ~VulkanSurface();

            // disable copy and move semantics to enforce unique ownership
            VulkanSurface(const VulkanSurface&) = delete;
            VulkanSurface& operator=(const VulkanSurface&) = delete;
            VulkanSurface(VulkanSurface&&) = delete;
            VulkanSurface& operator=(VulkanSurface&&) = delete;

            // accessors
            VkSurfaceKHR get() const noexcept;
            bool isValid() const noexcept;

            // query surface support details 
            bool canQueueFamilyPresent(VkPhysicalDevice vkPhysicalDevice, uint32_t queueFamilyIndex) const noexcept; 
            std::vector<VkSurfaceFormatKHR> getSupportedFormats(VkPhysicalDevice vkPhysicalDevice) const noexcept;
            std::vector<VkPresentModeKHR> getSupportedPresentModes(VkPhysicalDevice vkPhysicalDevice) const noexcept; 
            VkSurfaceCapabilitiesKHR getCapabilities(VkPhysicalDevice vkPhysicalDevice) const noexcept;

        private:
            // construction helpers
            VulkanSurface() noexcept;
            bool initialize(const VulkanInstance& instance, const Platform& platform) noexcept;

        private:
            // vulkan handles
            VkSurfaceKHR m_vkSurfaceKHR;
            VkInstance m_vkInstance;
    };
}   // namespace keplar
