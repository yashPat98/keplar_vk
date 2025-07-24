// ────────────────────────────────────────────
//  File: vulkan_swapchain.hpp · Created by Yash Patel · 7-11-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "vk_config.hpp"
#include "vulkan_surface.hpp"
#include "vulkan_device.hpp"

namespace keplar
{
    class VulkanSwapchain final 
    {
        public:
            // creation and destruction
            static std::unique_ptr<VulkanSwapchain> create(const VulkanSurface& surface, 
                                                           const VulkanDevice& device, 
                                                           VkExtent2D windowExtent) noexcept;
            ~VulkanSwapchain();

            // disable copy and move semantics to enforce unique ownership
            VulkanSwapchain(const VulkanSwapchain&) = delete;
            VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
            VulkanSwapchain(VulkanSwapchain&&) = delete;
            VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

            // accessors
            VkSwapchainKHR get() const noexcept;
            VkFormat getFormat() const noexcept;
            VkColorSpaceKHR getColorSpace() const noexcept;
            VkPresentModeKHR getPresentMode() const noexcept;
            VkExtent2D getExtent() const noexcept;
            uint32_t getImageCount() const noexcept;

            const std::vector<VkImage>& getImages() const noexcept; 
            const std::vector<VkImageView>& getImageViews() const noexcept;
 
        private:
            // construction helpers
            VulkanSwapchain() noexcept;
            bool initialize(const VulkanSurface& surface, const VulkanDevice& device, VkExtent2D windowExtent) noexcept;
            void chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) noexcept;
            void choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) noexcept;
            void chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept;
            void chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkExtent2D windowExtent) noexcept;
            void choosePreTransform(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept;
            bool createSwapchain(VkSurfaceKHR surface, QueueFamilyIndices indices) noexcept;
            bool recreateSwapchain(const VulkanSurface& surface, const VulkanDevice& device, VkExtent2D windowExtent) noexcept;
            bool createImageViews() noexcept;

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
