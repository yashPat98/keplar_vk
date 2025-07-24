// ────────────────────────────────────────────
//  File: vulkan_surface.cpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#include "vulkan_surface.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    std::unique_ptr<VulkanSurface> VulkanSurface::create(const VulkanInstance& instance, const Platform& platform) noexcept
    {
        std::unique_ptr<VulkanSurface> surface(new VulkanSurface);
        if (!surface->initialize(instance, platform))
        {
            return nullptr;
        }
        return surface;
    }

    VulkanSurface::VulkanSurface() noexcept
        : m_vkSurfaceKHR(VK_NULL_HANDLE)
        , m_vkInstance(VK_NULL_HANDLE)
    {
    }

    VulkanSurface::~VulkanSurface()
    {
        if (m_vkSurfaceKHR != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_vkInstance, m_vkSurfaceKHR, nullptr);
            m_vkSurfaceKHR = VK_NULL_HANDLE;
            m_vkInstance = VK_NULL_HANDLE;
            VK_LOG_INFO("presentation surface destroyed successfully");
        }
    }

    bool VulkanSurface::initialize(const VulkanInstance& instance, const Platform& platform) noexcept
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

    bool VulkanSurface::canQueueFamilyPresent(VkPhysicalDevice vkPhysicalDevice, uint32_t queueFamilyIndex) const noexcept
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

    std::vector<VkSurfaceFormatKHR> VulkanSurface::getSupportedFormats(VkPhysicalDevice vkPhysicalDevice) const noexcept    
    {
        if (vkPhysicalDevice == VK_NULL_HANDLE)
        {
            VK_LOG_WARN("getSupportedFormats called with VK_NULL_HANDLE");
            return {};
        }

        // query the count of supported surface formats
        uint32_t formatCount = 0;
        VkResult vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, m_vkSurfaceKHR, &formatCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("failed to query surface format count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return {};
        }

        // retrieve the supported surface formats
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, m_vkSurfaceKHR, &formatCount, surfaceFormats.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("failed to retrieve surface formats : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return {};
        }

        return surfaceFormats;
    }

    std::vector<VkPresentModeKHR> VulkanSurface::getSupportedPresentModes(VkPhysicalDevice vkPhysicalDevice) const noexcept
    {
        if (vkPhysicalDevice == VK_NULL_HANDLE)
        {
            VK_LOG_WARN("getSupportedPresentModes called with VK_NULL_HANDLE");
            return {};
        }

        // query the count of supported surface present modes 
        uint32_t presentModeCount = 0;
        VkResult vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, m_vkSurfaceKHR, &presentModeCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("failed to query surface present mode count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return {};
        }

        // retrieve the supported surface present modes 
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, m_vkSurfaceKHR, &presentModeCount, presentModes.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("failed to retrieve surface present modes : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return {};
        }

        return presentModes;
    }

    VkSurfaceCapabilitiesKHR VulkanSurface::getCapabilities(VkPhysicalDevice vkPhysicalDevice) const noexcept
    {
        VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR{};
        if (vkPhysicalDevice == VK_NULL_HANDLE)
        {
            VK_LOG_WARN("getCapabilities called with VK_NULL_HANDLE");
            return vkSurfaceCapabilitiesKHR;
        }

        // retrieve the surface capabilities
        VkResult vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, m_vkSurfaceKHR, &vkSurfaceCapabilitiesKHR);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("failed to retrieve surface capabilities : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return vkSurfaceCapabilitiesKHR;
        }

        return vkSurfaceCapabilitiesKHR;
    }

    VkSurfaceKHR VulkanSurface::get() const noexcept
    {
        return m_vkSurfaceKHR;
    }

    bool VulkanSurface::isValid() const noexcept
    {
        return (m_vkSurfaceKHR != VK_NULL_HANDLE);
    }
}   // namespace keplar
