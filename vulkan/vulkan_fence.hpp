// ────────────────────────────────────────────
//  File: vulkan_fence.hpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan_config.hpp"

namespace keplar
{
    class VulkanFence
    {
        public:
            // creation and destruction
            VulkanFence() noexcept;
            ~VulkanFence();

            // disable copy semantics to enforce unique ownership
            VulkanFence(const VulkanFence&) = delete;
            VulkanFence& operator=(const VulkanFence&) = delete;

            // move semantics
            VulkanFence(VulkanFence&&) noexcept;
            VulkanFence& operator=(VulkanFence&&) noexcept;

            // usage
            bool initialize(VkDevice vkDevice, const VkFenceCreateInfo& createInfo) noexcept;
            void destroy();
            bool wait(uint64_t timeout = UINT64_MAX) const noexcept;
            bool reset() const noexcept;
            VkResult isSignaled() const noexcept;

            // accessor
            VkFence get() const noexcept { return m_vkFence; }
            bool isValid() const noexcept { return m_vkFence != VK_NULL_HANDLE; }

        private:
            VkDevice m_vkDevice;
            VkFence m_vkFence;
    };  
}   // namespace keplar
