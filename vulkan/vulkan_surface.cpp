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

    bool VulkanSurface::initialize(const VulkanInstance& instance, const Platform& platform)
    {
        // store the VkInstance for resource creation and destruction
        m_vkInstance = instance.get();

        // get presentation surface from platform 
        m_vkSurfaceKHR = platform.createVulkanSurface(m_vkInstance);
        if (m_vkSurfaceKHR == VK_NULL_HANDLE)
        {   
            VK_LOG_FATAL("failed to create presentation surface");
            return false;
        }
        
        VK_LOG_INFO("presentation surface created successfully");
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

    bool VulkanSurface::canQueueFamilyPresent(VkPhysicalDevice vkPhysicalDevice, uint32_t queueFamilyIndex) const
    {
        if (vkPhysicalDevice == VK_NULL_HANDLE)
        {
            VK_LOG_WARN("canQueueFamilyPresent called with VK_NULL_HANDLE (queue family %u)", queueFamilyIndex);
            return false;
        }

        VkBool32 presentSupport = VK_FALSE;
        VkResult vkResult = vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice, queueFamilyIndex, m_vkSurfaceKHR, &presentSupport);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("surface support query failed (queue family %u): %s", queueFamilyIndex, string_VkResult(vkResult));
            return false;
        }

        return presentSupport == VK_TRUE;
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
