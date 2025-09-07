// ────────────────────────────────────────────
//  File: vulkan_swapchain.hpp · Created by Yash Patel · 7-11-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "vk_config.hpp"
#include "vulkan_context.hpp"
#include "vulkan_surface.hpp"
#include "vulkan_device.hpp"

namespace keplar
{
    class VulkanSwapchain final 
    {
        public:
            // creation and destruction
            VulkanSwapchain(const VulkanContext& context) noexcept;
            ~VulkanSwapchain();

            // disable copy and move semantics to enforce unique ownership
            VulkanSwapchain(const VulkanSwapchain&) = delete;
            VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
            VulkanSwapchain(VulkanSwapchain&&) = delete;
            VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

            bool initialize(uint32_t width, uint32_t height) noexcept;

            // accessors
            VkSwapchainKHR                  get() const noexcept                   { return m_vkSwapchainKHR; }
            const std::vector<VkImage>&     getColorImages() const noexcept        { return m_colorImages; }
            const std::vector<VkImageView>& getColorImageViews() const noexcept    { return m_colorImageViews; }
            VkImage                         getDepthImage() const noexcept         { return m_depthImage; }
            VkImageView                     getDepthImageView() const noexcept     { return m_depthImageView; }
            VkFormat                        getColorFormat() const noexcept        { return m_vkSurfaceFormatKHR.format; }
            VkFormat                        getDepthFormat() const noexcept        { return m_depthFormat; }
            VkColorSpaceKHR                 getColorSpace() const noexcept         { return m_vkSurfaceFormatKHR.colorSpace; }
            VkPresentModeKHR                getPresentMode() const noexcept        { return m_vkPresentModeKHR; }
            VkExtent2D                      getExtent() const noexcept             { return m_imageExtent; }
            uint32_t                        getImageCount() const noexcept         { return m_imageCount; }

        private:
            // initialization helpers
            bool chooseSurfaceFormat() noexcept;
            bool choosePresentMode() noexcept;
            void chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept;
            void chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkExtent2D windowExtent) noexcept;
            void choosePreTransform(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept;
            bool createSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) noexcept;
            bool createColorAttachment() noexcept;
            bool createDepthAttachment() noexcept;
            void destroyAttachments() noexcept;

        private:
            // dependencies
            const VulkanDevice& m_device;
            const VulkanSurface& m_surface;

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
            std::vector<VkImage> m_colorImages;
            std::vector<VkImageView> m_colorImageViews;

            VkImage m_depthImage;
            VkDeviceMemory m_depthDeviceMemory;
            VkImageView m_depthImageView;
            VkFormat m_depthFormat;
    };
}   // namespace keplar
