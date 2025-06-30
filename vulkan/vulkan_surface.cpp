// ────────────────────────────────────────────
//  File: vulkan_surface.cpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#include "vulkan_surface.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanSurface::VulkanSurface()
        : m_vkSurfaceKHR(VK_NULL_HANDLE)
        , m_vkInstance(VK_NULL_HANDLE)
    {
    }

    VulkanSurface::~VulkanSurface()
    {
        destroy();
    }

    bool VulkanSurface::initialize(VkInstance vkInstance, const Platform& platform)
    {
        // get presentation surface from platform 
        m_vkSurfaceKHR = platform.createVulkanSurface(vkInstance);
        if (m_vkSurfaceKHR == VK_NULL_HANDLE)
        {   
            VK_LOG_FATAL("failed to create presentation surface");
            return false;
        }
        
        // store instance reference for destruction
        VK_LOG_INFO("presentation surface created successfully");
        m_vkInstance = vkInstance;
        return true;
    }

    void VulkanSurface::destroy()
    {
        if (m_vkSurfaceKHR != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_vkInstance, m_vkSurfaceKHR, nullptr);
            m_vkSurfaceKHR = VK_NULL_HANDLE;
            VK_LOG_INFO("presentation surface destroyed successfully");
        }

        // clear instance reference
        m_vkInstance = VK_NULL_HANDLE;
    }

    VkSurfaceKHR VulkanSurface::get() const
    {
        return m_vkSurfaceKHR;
    }

    bool VulkanSurface::isValid() const
    {
        return (m_vkSurfaceKHR != VK_NULL_HANDLE);
    }
}   // namespace keplar
