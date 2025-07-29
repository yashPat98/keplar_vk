// ────────────────────────────────────────────
//  File: renderer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_command_buffer.hpp"
#include "vulkan/vulkan_render_pass.hpp"
#include "vulkan/vulkan_framebuffer.hpp"

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
            bool createFramebuffers();

        private:
            // immutable core vulkan components provided by context
            const VulkanDevice&                 m_vulkanDevice;
            const VulkanSwapchain&              m_vulkanSwapchain;

            // vulkan handles
            VkDevice                            m_vkDevice;

            // rendering resources and state
            uint32_t                            m_swapchainImageCount;
            VulkanCommandPool                   m_commandPool;
            std::vector<VulkanCommandBuffer>    m_commandBuffers;
            VulkanRenderPass                    m_renderPass;
            std::vector<VulkanFramebuffer>      m_framebuffers;
    };
}   // namespace keplar
