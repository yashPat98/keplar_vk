// ────────────────────────────────────────────
//  File: vulkan_command_pool.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vulkan_config.hpp"
#include <optional>

namespace keplar
{
    // forward declarations
    class VulkanCommandBuffer;

    class VulkanCommandPool final
    {
        public:
            // creation and destruction
            VulkanCommandPool() noexcept;
            ~VulkanCommandPool();

            // disable copy semantics to enforce unique ownership
            VulkanCommandPool(const VulkanCommandPool&) = delete;
            VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

            // move semantics
            VulkanCommandPool(VulkanCommandPool&&) noexcept;
            VulkanCommandPool& operator=(VulkanCommandPool&&) noexcept; 
            
            // pool operations
            bool initialize(VkDevice vkDevice, const VkCommandPoolCreateInfo& createInfo) noexcept;
            bool reset(VkCommandPoolResetFlags flags = 0) const noexcept;
            void trim(VkCommandPoolTrimFlags flags = 0) const noexcept;

            // command buffer allocation
            std::optional<VulkanCommandBuffer> allocateBuffer(VkCommandBufferLevel level) const noexcept;
            std::vector<VulkanCommandBuffer> allocateBuffers(uint32_t count, VkCommandBufferLevel level) const noexcept;
            void releaseBuffers(const std::vector<VulkanCommandBuffer>& commandBuffers) const noexcept;
            void releaseBuffer(const VulkanCommandBuffer& commandBuffer) const noexcept;

            // accessors
            VkCommandPool get() const noexcept { return m_vkCommandPool; }
            std::optional<uint32_t> getQueueFamilyIndex() const noexcept { return m_queueFamilyIndex; }

        private:
            VkDevice m_vkDevice;
            VkCommandPool m_vkCommandPool;
            std::optional<uint32_t> m_queueFamilyIndex;
    };
}   // namespace keplar
 