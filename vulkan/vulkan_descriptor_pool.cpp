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

    bool VulkanDescriptorPool::initialize(VkDevice vkDevice, const VkDescriptorPoolCreateInfo& createInfo) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanDescriptorPool::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        VkResult vkResult = vkCreateDescriptorPool(vkDevice, &createInfo, nullptr, &m_vkDescriptorPool);
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
