// ────────────────────────────────────────────
//  File: vulkan_render_pass.hpp · Created by Yash Patel · 7-27-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "vk_config.hpp"
#include "vulkan_device.hpp"

namespace keplar
{
    class VulkanRenderPass final
    {
        public:
            // creation and destruction
            VulkanRenderPass() noexcept;
            ~VulkanRenderPass();

            // disable copy semantics to enforce unique ownership
            VulkanRenderPass(const VulkanRenderPass&) = delete;
            VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

            // move semantics
            VulkanRenderPass(VulkanRenderPass&&) noexcept;
            VulkanRenderPass& operator=(VulkanRenderPass&&) noexcept;

            bool initialize(VkDevice vkDevice,
                            const std::vector<VkAttachmentDescription>& attachments,
                            const std::vector<VkSubpassDescription>& subpasses,
                            const std::vector<VkSubpassDependency>& dependencies = {}) noexcept;
            void destroy() noexcept;

            // accessor
            VkRenderPass get() const noexcept { return m_vkRenderPass; }

        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkRenderPass m_vkRenderPass;
    };  
}   // namespace keplar
