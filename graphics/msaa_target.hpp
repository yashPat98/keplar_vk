// ────────────────────────────────────────────
//  File: msaa_target.hpp · Created by Yash Patel · 9-12-2025
// ────────────────────────────────────────────

#pragma once 

#include "vulkan/vulkan_config.hpp"

namespace keplar
{
    // forward declarations
    class VulkanDevice;
    class VulkanSwapchain;

    class MsaaTarget
    {
        public:
            // creation and destruction
            MsaaTarget() noexcept;
            ~MsaaTarget();

            // disable copy semantics to enforce unique ownership
            MsaaTarget(const MsaaTarget&) = delete;
            MsaaTarget& operator=(const MsaaTarget&) = delete;

            // move semantics
            MsaaTarget(MsaaTarget&&) noexcept;
            MsaaTarget& operator=(MsaaTarget&&) noexcept;

            // usage 
            bool initialize(const VulkanDevice& device, const VulkanSwapchain& swapchain, VkSampleCountFlagBits desiredSampleCount) noexcept;
            void destroy() noexcept;

            // accessors
            VkImage getColorImage() const noexcept { return m_colorImage; }
            VkImage getDepthImage() const noexcept { return m_depthImage; }
            VkImageView getColorImageView() const noexcept { return m_colorImageView; }
            VkImageView getDepthImageView() const noexcept { return m_depthImageView; }
            VkExtent2D getExtent() const noexcept { return m_extent; }
            VkSampleCountFlagBits getSampleCount() const noexcept { return m_sampleCount; }
            
        private:
            // helper functions for msaa setup
            bool chooseSampleCount(const VkPhysicalDeviceProperties& deviceProperties, VkSampleCountFlagBits desiredSampleCount) noexcept;
            bool createColorTarget(const VulkanDevice& device, const VulkanSwapchain& swapchain) noexcept;
            bool createDepthTarget(const VulkanDevice& device, const VulkanSwapchain& swapchain) noexcept;

        private:
            // vulkan handles
            VkDevice                m_vkDevice;

            // color target 
            VkImage                 m_colorImage;
            VkDeviceMemory          m_colorMemory;
            VkImageView             m_colorImageView;

            // depth-stencil target
            VkImage                 m_depthImage;
            VkDeviceMemory          m_depthMemory;
            VkImageView             m_depthImageView;

            // msaa properties
            VkExtent2D              m_extent;
            VkSampleCountFlagBits   m_sampleCount;
    };
}   // namespace keplar
