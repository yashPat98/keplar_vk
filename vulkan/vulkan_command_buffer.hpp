// ────────────────────────────────────────────
//  File: vulkan_command_buffer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vk_config.hpp"

namespace keplar
{
    // forward declarations
    class VulkanCommandPool;

    class VulkanCommandBuffer final
    {
        public:
            // creation and destruction
            VulkanCommandBuffer() = delete;
            ~VulkanCommandBuffer() = default;

            // disable copy semantics
            VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
            VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

            // move semantics
            VulkanCommandBuffer(VulkanCommandBuffer&&) noexcept;
            VulkanCommandBuffer& operator=(VulkanCommandBuffer&&) noexcept; 

            // command recording
            bool begin(VkCommandBufferUsageFlags flags = 0) noexcept;
            bool end() noexcept;
            bool reset(VkCommandBufferResetFlags flags = 0) noexcept;

            // accessor
            VkCommandBuffer get() const noexcept { return m_vkCommandBuffer; }

        private:
            // only command pool can construct
            friend class VulkanCommandPool;
            explicit VulkanCommandBuffer(VkCommandBuffer vkCommandBuffer) noexcept;

        private:
            VkCommandBuffer m_vkCommandBuffer;
    };
}   // namespace keplar

