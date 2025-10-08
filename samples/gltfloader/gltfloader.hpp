// ────────────────────────────────────────────
//  File: gltfloader.hpp · Created by Yash Patel · 10-07-2025
// ────────────────────────────────────────────

#pragma once 

#include <vector>
#include <atomic>

#include "graphics/renderer.hpp"
#include "platform/platform.hpp"
#include "vulkan/vulkan_device.hpp"
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
#include "vulkan/vulkan_samplers.hpp"

#include "graphics/msaa_target.hpp"
#include "graphics/camera.hpp"
#include "graphics/gltf_model.hpp"
#include "shader_structs.hpp"

namespace keplar
{
    class GLTFLoader : public Renderer
    {
        public:
            // creation and destruction
            GLTFLoader() noexcept;
            ~GLTFLoader();

            // core renderer interface: initialize, update, render
            virtual bool initialize(std::weak_ptr<Platform> platform, std::weak_ptr<VulkanContext> context) noexcept override;
            virtual bool update(float dt) noexcept override;
            virtual bool renderFrame() noexcept override;

            // handle window and user input events
            virtual void onWindowResize(uint32_t, uint32_t) override;

        private:
            bool createSwapchain() noexcept;
            bool createMsaaTarget(const VulkanDevice& device) noexcept;
            bool createCommandPool(const VulkanDevice& device) noexcept;
            bool createCommandBuffers() noexcept;
            bool createTextureSamplers(const VulkanDevice& device) noexcept;
            bool loadAssets(const VulkanDevice& device) noexcept;
            bool createUniformBuffers(const VulkanDevice& device) noexcept;
            bool createShaderModules() noexcept;
            bool createDescriptorSetLayouts() noexcept;
            bool createDescriptorPool() noexcept;
            bool createDescriptorSets() noexcept;
            bool createRenderPasses() noexcept; 
            bool createGraphicsPipeline() noexcept;
            bool createFramebuffers() noexcept;
            bool createSyncPrimitives() noexcept;
            bool buildCommandBuffers() noexcept;
            bool prepareScene() noexcept;
            bool updateUniformBuffer() noexcept;

        private:
            // per frame sync primitives
            struct FrameSyncPrimitives
            {
                VulkanSemaphore  mImageAvailableSemaphore;
                VulkanSemaphore  mRenderCompleteSemaphore;
                VulkanFence      mInFlightFence;
            };

            // core dependencies
            std::weak_ptr<Platform>             m_platform;
            std::weak_ptr<VulkanContext>        m_context;
            std::weak_ptr<VulkanDevice>         m_device;

            // swapchain for presentation
            std::unique_ptr<VulkanSwapchain>    m_swapchain;   

            // vulkan handles
            VkDevice                            m_vkDevice;
            VkQueue                             m_presentQueue;
            VkQueue                             m_graphicsQueue;
            VkSwapchainKHR                      m_vkSwapchainKHR;

            // window dimensions
            uint32_t                            m_windowWidth;
            uint32_t                            m_windowHeight;

            // multisampling target 
            MsaaTarget                          m_msaaTarget;      
            VkSampleCountFlagBits               m_sampleCount; 

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
            VulkanSamplers                      m_samplers;     
            VulkanDescriptorSetLayout           m_descriptorSetLayout;
            VulkanDescriptorPool                m_descriptorPool;
            VulkanPipeline                      m_graphicsPipeline;

            // main camera and uniform buffer
            std::shared_ptr<Camera>             m_camera;
            std::vector<VulkanBuffer>           m_uniformBuffers;

            // descriptor sets and UBOs
            std::vector<VkDescriptorSet>        m_descriptorSets;
            std::vector<ubo::FrameData>         m_uboFrameData;

            // gltf model
            GLTFModel                           m_gltfModel;

    };
}   // namespace keplar
