// ────────────────────────────────────────────
//  File: vulkan_descriptor_pool.cpp · Created by Yash Patel · 8-25-2025
// ────────────────────────────────────────────

#include "vulkan/vulkan_descriptor_pool.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanDescriptorPool::VulkanDescriptorPool() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkDescriptorPool(VK_NULL_HANDLE)
        , m_requirements{}
    {
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        // destroy descriptor pool
        if (m_vkDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_vkDevice, m_vkDescriptorPool, nullptr);
            m_vkDescriptorPool = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_DEBUG("descriptor pool destroyed successfully");
        }
    }

    VulkanDescriptorPool::VulkanDescriptorPool(VulkanDescriptorPool&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkDescriptorPool(other.m_vkDescriptorPool)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkDescriptorPool = VK_NULL_HANDLE;
    }
    
    VulkanDescriptorPool& VulkanDescriptorPool::operator=(VulkanDescriptorPool&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            if (m_vkDescriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(m_vkDevice, m_vkDescriptorPool, nullptr);
            }

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkDescriptorPool = other.m_vkDescriptorPool;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkDescriptorPool = VK_NULL_HANDLE;
        }
        
        return *this;
    }

    void VulkanDescriptorPool::addRequirements(const DescriptorRequirements& requirement) noexcept
    {
        // add requirements to aggregate totals
        m_requirements.mMaxSets         += requirement.mMaxSets;
        m_requirements.mUniformCount    += requirement.mUniformCount;
        m_requirements.mSamplerCount    += requirement.mSamplerCount;
    }

    bool VulkanDescriptorPool::initialize(VkDevice vkDevice) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanDescriptorPool::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // prepare vulkan descriptor pool sizes based on aggregated requirements
        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(2);
        if (m_requirements.mUniformCount)
        {
            VkDescriptorPoolSize uniformPoolSize{};
            uniformPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformPoolSize.descriptorCount = m_requirements.mUniformCount;
            poolSizes.emplace_back(std::move(uniformPoolSize));
        }
        if (m_requirements.mSamplerCount)
        {
            VkDescriptorPoolSize samplerPoolSize{};
            samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerPoolSize.descriptorCount = m_requirements.mSamplerCount;
            poolSizes.emplace_back(std::move(samplerPoolSize));
        }

        // descriptor pool creation info structure
        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes = poolSizes.data();
        poolCreateInfo.maxSets = m_requirements.mMaxSets;

        // create the vulkan descriptor pool
        VkResult vkResult = vkCreateDescriptorPool(vkDevice, &poolCreateInfo, nullptr, &m_vkDescriptorPool);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("VulkanDescriptorPool failed to create descriptor pool : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device handle for destruction
        m_vkDevice = vkDevice;
        VK_LOG_DEBUG("descriptor pool created successfully");
        return true;
    }
}   // namespace keplar
