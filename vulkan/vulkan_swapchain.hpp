// ────────────────────────────────────────────
//  File: vulkan_swapchain.hpp · Created by Yash Patel · 7-11-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>
#include "vulkan_config.hpp"

namespace keplar
{
    // forward declarations
    class VulkanContext;
    class VulkanSurface;
    class VulkanDevice;
    class VulkanCommandBuffer;
    struct QueueFamilyIndices;

    class VulkanSwapchain final 
    {
        public:
            // creation and destruction
            VulkanSwapchain(std::weak_ptr<VulkanSurface> surface, std::weak_ptr<VulkanDevice> device) noexcept;
            ~VulkanSwapchain();

            // disable copy and move semantics to enforce unique ownership
            VulkanSwapchain(const VulkanSwapchain&) = delete;
            VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
            VulkanSwapchain(VulkanSwapchain&&) = delete;
            VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

            bool initialize(uint32_t width, uint32_t height) noexcept;
            bool recreate(uint32_t width, uint32_t height) noexcept;
            void destroy() noexcept;

            // accessors
            VkSwapchainKHR                  get() const noexcept                   { return m_vkSwapchainKHR; }
            
            const std::vector<VkImage>&     getColorImages() const noexcept        { return m_colorImages; }
            const std::vector<VkImageView>& getColorImageViews() const noexcept    { return m_colorImageViews; }
            VkImage                         getDepthImage() const noexcept         { return m_depthImage; }
            VkImageView                     getDepthImageView() const noexcept     { return m_depthImageView; }
            uint32_t                        getImageCount() const noexcept         { return m_imageCount; }
            VkExtent2D                      getExtent() const noexcept             { return m_imageExtent; }
            VkFormat                        getColorFormat() const noexcept        { return m_vkSurfaceFormatKHR.format; }
            VkFormat                        getDepthFormat() const noexcept        { return m_depthFormat; }
            VkColorSpaceKHR                 getColorSpace() const noexcept         { return m_vkSurfaceFormatKHR.colorSpace; }
            VkPresentModeKHR                getPresentMode() const noexcept        { return m_vkPresentModeKHR; }

            // swapchain image layout transition helpers
            void transitionForRendering(VulkanCommandBuffer commandBuffer, uint32_t imageIndex) const noexcept;
            void transitionForPresentation(VulkanCommandBuffer commandBuffer, uint32_t imageIndex) const noexcept;

        private:
            // initialization helpers
            bool chooseSurfaceFormat(const VulkanSurface& surface, const VulkanDevice& device) noexcept;
            bool choosePresentMode(const VulkanSurface& surface, const VulkanDevice& device) noexcept;
            void chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept;
            void chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkExtent2D windowExtent) noexcept;
            void choosePreTransform(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept;
            bool createSwapchain(VkSurfaceKHR vkSurface, QueueFamilyIndices indices, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) noexcept;
            bool createColorAttachment() noexcept;
            bool createDepthAttachment(const VulkanDevice& device) noexcept;
            void destroyAttachments() noexcept;

        private:
            // dependencies
            std::weak_ptr<VulkanSurface> m_surface;
            std::weak_ptr<VulkanDevice>  m_device;

            // vulkan handles
            VkDevice        m_vkDevice;
            VkSwapchainKHR  m_vkSwapchainKHR;

            // swapchain properties
            VkSurfaceFormatKHR            m_vkSurfaceFormatKHR;
            VkPresentModeKHR              m_vkPresentModeKHR;
            uint32_t                      m_imageCount;
            VkExtent2D                    m_imageExtent;
            VkSurfaceTransformFlagBitsKHR m_preTransform;

            // color attachments
            std::vector<VkImage>        m_colorImages;
            std::vector<VkImageView>    m_colorImageViews;

            // depth attachments
            VkImage                     m_depthImage;
            VkDeviceMemory              m_depthMemory;
            VkImageView                 m_depthImageView;
            VkFormat                    m_depthFormat;
    };
}   // namespace keplar
