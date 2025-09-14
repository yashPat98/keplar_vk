// ────────────────────────────────────────────
//  File: vulkan_pipeline.hpp · Created by Yash Patel · 8-19-2025
// ────────────────────────────────────────────

#pragma once

#include <optional>
#include "vulkan_config.hpp"

namespace keplar
{
    struct GraphicsPipelineConfig
    {
        // core
        VkPipelineCreateFlags                                   mFlags{};                       // pipeline creation flags
        std::vector<VkPipelineShaderStageCreateInfo>            mShaderStages;                  // shader stages (vert/frag/geom/tess)
        
        // fixed-function state
        VkPipelineVertexInputStateCreateInfo                    mVertexInputState{};            // vertex input bindings & attributes
        VkPipelineInputAssemblyStateCreateInfo                  mInputAssemblyState{};          // primitive topology & restart
        std::optional<VkPipelineTessellationStateCreateInfo>    mTessellationState{};           // tessellation state (optional)
        VkPipelineViewportStateCreateInfo                       mViewportState{};               // viewports & scissors
        VkPipelineRasterizationStateCreateInfo                  mRasterizationState{};          // polygon mode, cull face, front face
        VkPipelineMultisampleStateCreateInfo                    mMultisampleState{};            // multisampling (MSAA)
        std::optional<VkPipelineDepthStencilStateCreateInfo>    mDepthStencilState{};           // depth & stencil test
        VkPipelineColorBlendStateCreateInfo                     mColorBlendState{};             // color blending
        std::optional<VkPipelineDynamicStateCreateInfo>         mDynamicState{};                // dynamic states (viewport, scissor, etc.)
        
        // subpass binding
        VkRenderPass                                            mRenderPass{VK_NULL_HANDLE};    // render pass handle
        uint32_t                                                mSubpassIndex{0};               // subpass index
        
        // pipeline layout 
        std::vector<VkDescriptorSetLayout>                      mDescriptorSetLayouts;          // descriptor sets for pipeline layout
        std::vector<VkPushConstantRange>                        mPushConstantRanges;            // push constants for pipeline layout
    };
 
    class VulkanPipeline
    {
        public:   
            // creation and destruction
            VulkanPipeline() noexcept;
            ~VulkanPipeline();

            // disable copy semantics to enforce unique ownership
            VulkanPipeline(const VulkanPipeline&) = delete;
            VulkanPipeline& operator=(const VulkanPipeline&) = delete;

            // move semantics
            VulkanPipeline(VulkanPipeline&&) noexcept;
            VulkanPipeline& operator=(VulkanPipeline&&) noexcept;

            bool initialize(VkDevice vkDevice, const GraphicsPipelineConfig& pipelineConfig) noexcept;
            void destroy() noexcept;

            // accessor
            VkPipeline get() const noexcept { return m_vkPipeline; }
            VkPipelineLayout getLayout() const noexcept { return m_vkPipelineLayout; }
            bool isValid() const noexcept { return m_vkPipeline != VK_NULL_HANDLE; }

        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkPipeline m_vkPipeline;
            VkPipelineLayout m_vkPipelineLayout;
            VkPipelineCache m_vkPipelineCache;
    };
}   // namespace keplar
