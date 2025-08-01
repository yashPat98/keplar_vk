// ────────────────────────────────────────────
//  File: vulkan_semaphore.cpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#include "vulkan_semaphore.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanSemaphore::VulkanSemaphore() noexcept
        : m_vkSemaphore(VK_NULL_HANDLE)
        , m_vkDevice(VK_NULL_HANDLE)
    {
    }

    VulkanSemaphore::~VulkanSemaphore()
    {
        // destroy semaphore
        if (m_vkSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_vkDevice, m_vkSemaphore, nullptr);
            m_vkSemaphore = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_INFO("semaphore object destroyed successfully");
        }
    }

    bool VulkanSemaphore::initialize(VkDevice vkDevice, const VkSemaphoreCreateInfo& createInfo) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanSemaphore::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // create semaphore with provided creation info
        VkResult vkResult = vkCreateSemaphore(vkDevice, &createInfo, nullptr, &m_vkSemaphore);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateSemaphore failed to create semaphore : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device handle for destruction
        m_vkDevice = vkDevice;
        VK_LOG_INFO("semaphore object created successfully");
        return true;
    }
}   // namespace keplar
