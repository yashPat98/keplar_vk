// ────────────────────────────────────────────
//  File: renderer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_command_buffer.hpp"
#include "vulkan/vulkan_render_pass.hpp"

namespace keplar
{
    class Renderer final
    {
        public:
            // creation and destruction
            explicit Renderer(const VulkanContext& context) noexcept;
            ~Renderer();

            // disable copy and move semantics to enforce unique ownership
            Renderer(const Renderer&) = delete;
            Renderer& operator=(const Renderer&) = delete;
            Renderer(Renderer&&) = delete;
            Renderer& operator=(Renderer&&) = delete; 

            bool initialize() noexcept;

        private:
            bool createCommandBuffers();
            bool createRenderPasses();

        private:
            // immutable Vulkan handles owned by context
            const VulkanDevice&                 m_vulkanDevice;
            const VulkanSwapchain&              m_vulkanSwapchain;

            VulkanCommandPool                   m_commandPool;
            std::vector<VulkanCommandBuffer>    m_commandBuffers;
            VulkanRenderPass                    m_renderPass;
            uint32_t                            m_swapchainImageCount;
    };
}   // namespace keplar
