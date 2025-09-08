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

            // disable copy semantics to enforce unique ownership
            VulkanSemaphore(const VulkanSemaphore&) = delete;
            VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

            // move semantics 
            VulkanSemaphore(VulkanSemaphore&&) noexcept;
            VulkanSemaphore& operator=(VulkanSemaphore&&) noexcept;

            // usage
            bool initialize(VkDevice vkDevice, const VkSemaphoreCreateInfo& createInfo) noexcept;
            void destroy() noexcept;

            // accessor
            VkSemaphore get() const noexcept { return m_vkSemaphore; }
            bool isValid() const noexcept { return m_vkSemaphore != VK_NULL_HANDLE; }

        private:
            VkDevice m_vkDevice;
            VkSemaphore m_vkSemaphore;
    };
}   // namespace keplar
