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

            // disable copy and move semantics to enforce unique ownership
            VulkanFramebuffer(const VulkanFramebuffer&) = delete;
            VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;
            VulkanFramebuffer(VulkanFramebuffer&&) = delete;
            VulkanFramebuffer& operator=(VulkanFramebuffer&&) = delete;

            bool initialize(VkDevice vkDevice, const VkFramebufferCreateInfo& createInfo) noexcept;

            // accessors
            VkFramebuffer get() const noexcept { return m_vkFramebuffer; }
            VkExtent2D getExtent() const noexcept { return m_framebufferExtent; }

        private:
            // vulkan handles
            VkFramebuffer m_vkFramebuffer;
            VkDevice m_vkDevice;
            VkExtent2D m_framebufferExtent;
    };
}   // namespace keplar
