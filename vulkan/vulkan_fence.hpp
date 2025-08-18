// ────────────────────────────────────────────
//  File: vulkan_fence.hpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

namespace keplar
{
    class VulkanFence
    {
        public:
            // creation and destruction
            VulkanFence() noexcept;
            ~VulkanFence();

            // disable copy and move semantics to enforce unique ownership
            VulkanFence(const VulkanFence&) = delete;
            VulkanFence& operator=(const VulkanFence&) = delete;
            VulkanFence(VulkanFence&&) = delete;
            VulkanFence& operator=(VulkanFence&&) = delete;

            // usage
            bool initialize(VkDevice vkDevice, const VkFenceCreateInfo& createInfo) noexcept;
            bool wait(uint64_t timeout = UINT64_MAX) const noexcept;
            bool reset() noexcept;
            VkResult isSignaled() noexcept;

            // accessor
            VkFence get() const noexcept { return m_vkFence; }
            bool isValid() const noexcept { return m_vkFence != VK_NULL_HANDLE; }

        private:
            VkFence m_vkFence;
            VkDevice m_vkDevice;
    };  
}   // namespace keplar
