// ────────────────────────────────────────────
//  File: vulkan_semaphore.hpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

namespace keplar
{
    class VulkanSemaphore
    {
        public:
            // creation and destruction
            VulkanSemaphore() noexcept;
            ~VulkanSemaphore();

            // disable copy and move semantics to enforce unique ownership
            VulkanSemaphore(const VulkanSemaphore&) = delete;
            VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;
            VulkanSemaphore(VulkanSemaphore&&) = delete;
            VulkanSemaphore& operator=(VulkanSemaphore&&) = delete;

            // usage
            bool initialize(VkDevice vkDevice, const VkSemaphoreCreateInfo& createInfo) noexcept;

            // accessor
            VkSemaphore get() const noexcept { return m_vkSemaphore; }
            bool isValid() const noexcept { return m_vkSemaphore != VK_NULL_HANDLE; }

        private:
            VkSemaphore m_vkSemaphore;
            VkDevice m_vkDevice;
    };
}   // namespace keplar
