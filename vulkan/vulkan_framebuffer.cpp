// ────────────────────────────────────────────
//  File: vulkan_framebuffer.cpp · Created by Yash Patel · 7-28-2025
// ────────────────────────────────────────────

#include "vulkan_framebuffer.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanFramebuffer::VulkanFramebuffer() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkFramebuffer(VK_NULL_HANDLE)
    {
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        // destroy framebuffer
        if (m_vkFramebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_vkDevice, m_vkFramebuffer, nullptr);
            m_vkFramebuffer = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_INFO("vulkan framebuffer destroyed successfully");
        }
    }

    VulkanFramebuffer::VulkanFramebuffer(VulkanFramebuffer&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkFramebuffer(other.m_vkFramebuffer)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkFramebuffer = VK_NULL_HANDLE;
    }

    VulkanFramebuffer& VulkanFramebuffer::operator=(VulkanFramebuffer&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            if (m_vkFramebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_vkDevice, m_vkFramebuffer, nullptr);
            }

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkFramebuffer = other.m_vkFramebuffer;

            // reset the source
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkFramebuffer = VK_NULL_HANDLE;
        }

        return *this;
    }

    bool VulkanFramebuffer::initialize(VkDevice vkDevice, const VkFramebufferCreateInfo& createInfo) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanFramebuffer::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // create framebuffer with provided creation info
        VkResult vkResult = vkCreateFramebuffer(vkDevice, &createInfo, nullptr, &m_vkFramebuffer);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateFramebuffer failed to create framebuffer : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device handle for destruction
        m_vkDevice = vkDevice;
        m_framebufferExtent = { createInfo.width, createInfo.height };
        VK_LOG_INFO("vulkan framebuffer created successfully");
        return true;
    }
}   // namespace keplar
