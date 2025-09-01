// ────────────────────────────────────────────
//  File: vulkan_pipeline.cpp · Created by Yash Patel · 8-19-2025
// ────────────────────────────────────────────

#include "vulkan_pipeline.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanPipeline::VulkanPipeline() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkPipeline(VK_NULL_HANDLE)
        , m_vkPipelineLayout(VK_NULL_HANDLE)
        , m_vkPipelineCache(VK_NULL_HANDLE)
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_vkPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
            m_vkPipeline = VK_NULL_HANDLE;
        }

        if (m_vkPipelineCache != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(m_vkDevice, m_vkPipelineCache, nullptr);
            m_vkPipelineCache = VK_NULL_HANDLE;
        }

        if (m_vkPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
            m_vkPipelineLayout = VK_NULL_HANDLE;
        }
    }

    VulkanPipeline::VulkanPipeline(VulkanPipeline&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkPipeline(other.m_vkPipeline)
        , m_vkPipelineLayout(other.m_vkPipelineLayout)
        , m_vkPipelineCache(other.m_vkPipelineCache)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkPipeline = VK_NULL_HANDLE;
        other.m_vkPipelineLayout = VK_NULL_HANDLE;
        other.m_vkPipelineCache = VK_NULL_HANDLE;
    }

    VulkanPipeline& VulkanPipeline::operator=(VulkanPipeline&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            if (m_vkPipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
            }

            if (m_vkPipelineCache != VK_NULL_HANDLE)
            {
                vkDestroyPipelineCache(m_vkDevice, m_vkPipelineCache, nullptr);
            }

            if (m_vkPipelineLayout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
            }

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkPipeline = other.m_vkPipeline;
            m_vkPipelineLayout = other.m_vkPipelineLayout;
            m_vkPipelineCache = other.m_vkPipelineCache;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkPipeline = VK_NULL_HANDLE;
            other.m_vkPipelineLayout = VK_NULL_HANDLE;
            other.m_vkPipelineCache = VK_NULL_HANDLE;
        }
        
        return *this;
    }


    bool VulkanPipeline::initialize(VkDevice vkDevice, const GraphicsPipelineConfig& pipelineConfig) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanPipeline::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // pipeline layout creation info
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = nullptr;
        pipelineLayoutCreateInfo.flags = 0;
        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(pipelineConfig.mDescriptorSetLayouts.size());
        pipelineLayoutCreateInfo.pSetLayouts = pipelineConfig.mDescriptorSetLayouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pipelineConfig.mPushConstantRanges.size());
        pipelineLayoutCreateInfo.pPushConstantRanges = pipelineConfig.mPushConstantRanges.data();

        // create pipeline layout
        VkResult vkResult = vkCreatePipelineLayout(vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreatePipelineLayout failed to create pipeline layout : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // pipeline cache creation info (currently empty cache)
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        pipelineCacheCreateInfo.pNext = nullptr;
        pipelineCacheCreateInfo.flags = 0;

        // create pipeline cache
        vkResult = vkCreatePipelineCache(vkDevice, &pipelineCacheCreateInfo, nullptr, &m_vkPipelineCache);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreatePipelineCache failed to create pipeline cache : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }
 
        // graphics pipeline creation info
        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.pNext = nullptr;
        pipelineCreateInfo.flags = 0;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineConfig.mShaderStages.size());
        pipelineCreateInfo.pStages = pipelineConfig.mShaderStages.data();
        pipelineCreateInfo.pVertexInputState = &pipelineConfig.mVertexInputState; 
        pipelineCreateInfo.pInputAssemblyState = &pipelineConfig.mInputAssemblyState;
        pipelineCreateInfo.pViewportState = &pipelineConfig.mViewportState;
        pipelineCreateInfo.pRasterizationState = &pipelineConfig.mRasterizationState;
        pipelineCreateInfo.pMultisampleState = &pipelineConfig.mMultisampleState;
        pipelineCreateInfo.pColorBlendState = &pipelineConfig.mColorBlendState;
        pipelineCreateInfo.layout = m_vkPipelineLayout;
        pipelineCreateInfo.renderPass = pipelineConfig.mRenderPass;
        pipelineCreateInfo.subpass = pipelineConfig.mSubpassIndex;
        pipelineCreateInfo.basePipelineHandle = nullptr;
        pipelineCreateInfo.basePipelineIndex = 0;
   
        // optional states
        pipelineCreateInfo.pTessellationState = pipelineConfig.mTessellationState ? &(*pipelineConfig.mTessellationState) : nullptr;
        pipelineCreateInfo.pDepthStencilState = pipelineConfig.mDepthStencilState ? &(*pipelineConfig.mDepthStencilState) : nullptr;
        pipelineCreateInfo.pDynamicState      = pipelineConfig.mDynamicState      ? &(*pipelineConfig.mDynamicState)      : nullptr;
  
        // create graphics pipeline
        vkResult = vkCreateGraphicsPipelines(vkDevice, m_vkPipelineCache, 1, &pipelineCreateInfo, nullptr, &m_vkPipeline);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateGraphicsPipelines failed to create pipeline : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        VK_LOG_INFO("graphics pipeline created successfully");
        m_vkDevice = vkDevice;
        return true;
    }
}   // namespace keplar