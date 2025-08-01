// ────────────────────────────────────────────
//  File: renderer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include <vector>
#include <atomic>

#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_command_buffer.hpp"
#include "vulkan/vulkan_render_pass.hpp"
#include "vulkan/vulkan_framebuffer.hpp"
#include "vulkan/vulkan_fence.hpp"
#include "vulkan/vulkan_semaphore.hpp"

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
            bool renderFrame() noexcept;

        private:
            bool createCommandBuffers();
            bool createRenderPasses();
            bool createFramebuffers();
            bool createSyncPrimitives();
            bool buildCommandBuffers();

        private:
            // per frame sync primitives
            struct FrameSyncPrimitives
            {
                VulkanSemaphore  mImageAvailableSemaphore;
                VulkanSemaphore  mRenderCompleteSemaphore;
                VulkanFence      mInFlightFence;
            };

            // immutable core vulkan components provided by context
            const VulkanDevice&                 m_vulkanDevice;
            const VulkanSwapchain&              m_vulkanSwapchain;

            // vulkan handles
            VkDevice                            m_vkDevice;
            VkSwapchainKHR                      m_vkSwapchainKHR;
            VkQueue                             m_presentQueue;
            VkQueue                             m_graphicsQueue;

            // rendering resources and state
            uint32_t                            m_swapchainImageCount;
            uint32_t                            m_maxFramesInFlight;
            uint32_t                            m_currentImageIndex;
            uint32_t                            m_currentFrameIndex;
            VulkanCommandPool                   m_commandPool;
            std::vector<VulkanCommandBuffer>    m_commandBuffers;
            VulkanRenderPass                    m_renderPass;
            std::vector<VulkanFramebuffer>      m_framebuffers;
            std::vector<FrameSyncPrimitives>    m_frameSyncPrimitives;
            std::atomic<bool>                   m_readyToRender;
    };
}   // namespace keplar
