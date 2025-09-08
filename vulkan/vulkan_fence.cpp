// ────────────────────────────────────────────
//  File: vulkan_fence.cpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#include "vulkan_fence.hpp"
#include "vk_utils.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanFence::VulkanFence() noexcept
        : m_vkDevice(VK_NULL_HANDLE) 
        , m_vkFence(VK_NULL_HANDLE) 
    {
    }

    VulkanFence::~VulkanFence()
    {
        destroy();
    }

    VulkanFence::VulkanFence(VulkanFence&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkFence(other.m_vkFence)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkFence = VK_NULL_HANDLE;
    }

    VulkanFence& VulkanFence::operator=(VulkanFence&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            destroy();

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkFence = other.m_vkFence;

            // reset the source
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkFence = VK_NULL_HANDLE;
        }

        return *this;
    }

    bool VulkanFence::initialize(VkDevice vkDevice, const VkFenceCreateInfo& createInfo) noexcept
    {   
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanFence::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // create fence with provided creation info
        VkResult vkResult = vkCreateFence(vkDevice, &createInfo, nullptr, &m_vkFence);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateFence failed to create fence : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device handle for destruction
        m_vkDevice = vkDevice;
        VK_LOG_DEBUG("fence object created successfully");
        return true;

    }

    void VulkanFence::destroy()
    {
        // destroy vulkan fence
        if (m_vkFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_vkDevice, m_vkFence, nullptr);
            m_vkFence = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_DEBUG("fence object destroyed successfully");
        }
    }

    bool VulkanFence::wait(uint64_t timeout) const noexcept
    {
        if (m_vkFence == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanFence::wait failed : fence object is not initialized");
            return false;
        }
        
        return VK_CHECK(vkWaitForFences(m_vkDevice, 1, &m_vkFence, VK_TRUE, timeout));
    }

    bool VulkanFence::reset() noexcept
    {
        if (m_vkFence == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanFence::reset failed : fence object is not initialized");
            return false;
        }

        return VK_CHECK(vkResetFences(m_vkDevice, 1, &m_vkFence));
    }

    VkResult VulkanFence::isSignaled() noexcept
    {
        if (m_vkFence == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanFence::isSignaled failed : fence object is not initialized");
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        return vkGetFenceStatus(m_vkDevice, m_vkFence);
    }
}   // namespace keplar
