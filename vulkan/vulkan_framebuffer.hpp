// ────────────────────────────────────────────
//  File: vulkan_framebuffer.hpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

namespace keplar
{
    class VulkanFramebuffer
    {
        public:
            // creation and destruction
            VulkanFramebuffer() noexcept;
            ~VulkanFramebuffer();

            // disable copy semantics to enforce unique ownership
            VulkanFramebuffer(const VulkanFramebuffer&) = delete;
            VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

            // move semantics
            VulkanFramebuffer(VulkanFramebuffer&&) noexcept;
            VulkanFramebuffer& operator=(VulkanFramebuffer&&) noexcept;

            bool initialize(VkDevice vkDevice, const VkFramebufferCreateInfo& createInfo) noexcept;
            void destroy() noexcept;

            // accessors
            VkFramebuffer get() const noexcept { return m_vkFramebuffer; }
            VkExtent2D getExtent() const noexcept { return m_framebufferExtent; }

        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkFramebuffer m_vkFramebuffer;
            VkExtent2D m_framebufferExtent;
    };
}   // namespace keplar
