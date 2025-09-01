// ────────────────────────────────────────────
//  File: vulkan_descriptor_set_layout.cpp · Created by Yash Patel · 8-24-2025
// ────────────────────────────────────────────

#include "vulkan/vulkan_descriptor_set_layout.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkDescriptorSetLayout(VK_NULL_HANDLE)
    {
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        // destroy descriptor set layout
        if (m_vkDescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
            m_vkDescriptorSetLayout = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_INFO("descriptor set layout destroyed successfully");
        }
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkDescriptorSetLayout(other.m_vkDescriptorSetLayout)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    VulkanDescriptorSetLayout& VulkanDescriptorSetLayout::operator=(VulkanDescriptorSetLayout&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            if (m_vkDescriptorSetLayout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
            }

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkDescriptorSetLayout = other.m_vkDescriptorSetLayout;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkDescriptorSetLayout = VK_NULL_HANDLE;    
        }

        return *this;
    }


    bool VulkanDescriptorSetLayout::initialize(VkDevice vkDevice, const VkDescriptorSetLayoutCreateInfo& createInfo) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanDescriptorSetLayout::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        VkResult vkResult = vkCreateDescriptorSetLayout(vkDevice, &createInfo, nullptr, &m_vkDescriptorSetLayout);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateDescriptorSetLayout failed to create descriptor set layout : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device handle for destruction
        m_vkDevice = vkDevice;
        VK_LOG_DEBUG("descriptor set layout created successfully");
        return true;
    }
}   // namespace keplar
