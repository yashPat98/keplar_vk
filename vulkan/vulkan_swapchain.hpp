// ────────────────────────────────────────────
//  File: vulkan_swapchain.hpp · Created by Yash Patel · 7-11-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"
#include "vulkan_surface.hpp"
#include "vulkan_device.hpp"

namespace keplar
{
    class VulkanSwapchain final 
    {
        public:
            VulkanSwapchain();
            ~VulkanSwapchain();

            // disable copy and move semantics to enforce unique ownership
            VulkanSwapchain(const VulkanSwapchain&) = delete;
            VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
            VulkanSwapchain(VulkanSwapchain&&) = delete;
            VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

            bool initialize(const VulkanSurface& surface, const VulkanDevice& device, VkExtent2D windowExtent);
            void destroy();

            // accessors
            VkSwapchainKHR get() const;
            VkFormat getFormat() const;
            VkColorSpaceKHR getColorSpace() const;
            VkPresentModeKHR getPresentMode() const;
            VkExtent2D getExtent() const;
            uint32_t getImageCount() const;

            const std::vector<VkImage>& getImages() const;
            const std::vector<VkImageView>& getImageViews() const;
 
        private:
            void chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
            void choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
            void chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
            void chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkExtent2D windowExtent);
            void choosePreTransform(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
            bool createSwapchain(VkSurfaceKHR surface, QueueFamilyIndices indices);
            bool recreateSwapchain(const VulkanSurface& surface, const VulkanDevice& device, VkExtent2D windowExtent);
            bool createImageViews();

        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkSwapchainKHR m_vkSwapchainKHR;

            // swapchain properties
            VkSurfaceFormatKHR m_vkSurfaceFormatKHR;
            VkPresentModeKHR m_vkPresentModeKHR;
            uint32_t m_imageCount;
            VkExtent2D m_imageExtent;
            VkSurfaceTransformFlagBitsKHR m_preTransform;

            // swapchain resources
            std::vector<VkImage> m_images;
            std::vector<VkImageView> m_imageViews;
    };
}   // namespace keplar
