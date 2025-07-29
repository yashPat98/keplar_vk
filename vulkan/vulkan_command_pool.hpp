// ────────────────────────────────────────────
//  File: vulkan_command_pool.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vk_config.hpp"
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

            // disable copy and move semantics to enforce unique ownership
            VulkanCommandPool(const VulkanCommandPool&) = delete;
            VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
            VulkanCommandPool(VulkanCommandPool&&) = delete;
            VulkanCommandPool& operator=(VulkanCommandPool&&) = delete; 
            
            // pool operations
            bool initialize(VkDevice vkDevice, const VkCommandPoolCreateInfo& createInfo) noexcept;
            bool reset(VkCommandPoolResetFlags flags = 0) noexcept;
            void trim(VkCommandPoolTrimFlags flags = 0) noexcept;

            // command buffer allocation
            std::optional<VulkanCommandBuffer> allocate(VkCommandBufferLevel level) noexcept;
            std::vector<VulkanCommandBuffer> allocate(uint32_t count, VkCommandBufferLevel level) noexcept;
            void freeCommandBuffers(const std::vector<VulkanCommandBuffer>& commandBuffers) noexcept;

            // accessors
            VkCommandPool get() const noexcept { return m_vkCommandPool; }
            std::optional<uint32_t> getQueueFamilyIndex() const noexcept { return m_queueFamilyIndex; }

        private:
            VkDevice m_vkDevice;
            VkCommandPool m_vkCommandPool;
            std::optional<uint32_t> m_queueFamilyIndex;
            std::vector<VkCommandBuffer> m_ownedCommandBuffers;
    };
}   // namespace keplar
 