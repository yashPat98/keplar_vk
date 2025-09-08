// ────────────────────────────────────────────
//  File: renderer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include <vector>
#include <atomic>

#include "platform/event_listener.hpp"
#include "utils/thread_pool.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_swapchain.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_command_buffer.hpp"
#include "vulkan/vulkan_render_pass.hpp"
#include "vulkan/vulkan_framebuffer.hpp"
#include "vulkan/vulkan_fence.hpp"
#include "vulkan/vulkan_semaphore.hpp"
#include "vulkan/vulkan_buffer.hpp"
#include "vulkan/vulkan_shader.hpp"
#include "vulkan/vulkan_descriptor_set_layout.hpp"
#include "vulkan/vulkan_descriptor_pool.hpp"
#include "vulkan/vulkan_pipeline.hpp"
#include "shader_structs.hpp"

namespace keplar
{
    class Renderer final : public EventListener
    {
        public:
            // creation and destruction
            explicit Renderer(const VulkanContext& context, uint32_t winWidth, uint32_t winHeight) noexcept;
            ~Renderer();

            // disable copy and move semantics to enforce unique ownership
            Renderer(const Renderer&) = delete;
            Renderer& operator=(const Renderer&) = delete;
            Renderer(Renderer&&) = delete;
            Renderer& operator=(Renderer&&) = delete; 

            // initialize vulkan resources and submit frames for rendering
            bool initialize() noexcept;
            bool renderFrame() noexcept;

            // handle window and user input events
            virtual void onWindowResize(uint32_t, uint32_t) override;

        private:
            bool createSwapchain();
            bool createCommandPool();
            bool createCommandBuffers();
            bool createVertexBuffers();
            bool createUniformBuffers();
            bool createShaderModules();
            bool createDescriptorSetLayouts();
            bool createDescriptorPool();
            bool createDescriptorSets();
            bool createRenderPasses(); 
            bool createGraphicsPipeline();
            bool createFramebuffers();
            bool createSyncPrimitives();
            bool buildCommandBuffers();
            bool updateUniformBuffer();

        private:
            // per frame sync primitives
            struct FrameSyncPrimitives
            {
                VulkanSemaphore  mImageAvailableSemaphore;
                VulkanSemaphore  mRenderCompleteSemaphore;
                VulkanFence      mInFlightFence;
            };

            // dedicated thread pool 
            ThreadPool                          m_threadPool;

            // core dependencies
            const VulkanContext&                m_vulkanContext;
            const VulkanDevice&                 m_vulkanDevice;
            
            // swapchain for presentation
            VulkanSwapchain                     m_vulkanSwapchain; 

            // vulkan handles
            VkDevice                            m_vkDevice;
            VkQueue                             m_presentQueue;
            VkQueue                             m_graphicsQueue;
            VkSwapchainKHR                      m_vkSwapchainKHR;

            // window dimensions
            uint32_t                            m_windowWidth;
            uint32_t                            m_windowHeight;

            // rendering state
            uint32_t                            m_swapchainImageCount;
            uint32_t                            m_maxFramesInFlight;
            uint32_t                            m_currentImageIndex;
            uint32_t                            m_currentFrameIndex;
            std::atomic<bool>                   m_readyToRender;

            // command buffers and synchronization
            VulkanCommandPool                   m_commandPool;
            std::vector<VulkanCommandBuffer>    m_commandBuffers;
            VulkanRenderPass                    m_renderPass;
            std::vector<VulkanFramebuffer>      m_framebuffers;
            std::vector<FrameSyncPrimitives>    m_frameSyncPrimitives;
            
            // shaders and pipeline
            VulkanShader                        m_vertexShader;
            VulkanShader                        m_fragmentShader;
            VulkanDescriptorSetLayout           m_descriptorSetLayout;
            VulkanDescriptorPool                m_descriptorPool;
            VulkanPipeline                      m_graphicsPipeline;

            // buffers
            VulkanBuffer                        m_positionBuffer;
            VulkanBuffer                        m_colorBuffer;
            std::vector<VulkanBuffer>           m_uniformBuffers;

            // descriptor sets and UBOs
            std::vector<VkDescriptorSet>        m_descriptorSets;
            std::vector<ubo::FrameData>         m_uboFrameData;
            
            // initial uniform data
            glm::mat4                           m_projectionMatrix;
    };
}   // namespace keplar
