// ────────────────────────────────────────────
//  File: vulkan_context.cpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#include "vulkan_context.hpp"

namespace keplar
{
    VulkanContext::VulkanContext()
        : m_vulkanInstance(nullptr)
        , m_vulkanSurface(nullptr)
    {
    }

    VulkanContext::~VulkanContext()
    {
    }

    bool VulkanContext::initialize(const Platform& platform, const VulkanContextConfig& config)
    {
        // construct vulkan subsystem wrapper objects
        m_vulkanInstance = std::make_unique<VulkanInstance>();
        m_vulkanSurface = std::make_unique<VulkanSurface>();

        // create vulkan instance based on config
        if (!m_vulkanInstance->initialize(config.mExtensions, config.mValidationLayers))
        {
            return false;
        }
        
        // create presentation surface 
        if (!m_vulkanSurface->initialize(m_vulkanInstance->get(), platform))
        {
            return false;
        }

        return true;
    }

    void VulkanContext::destroy()
    {
        // destroy vulkan resources in reverse order of creation for safe cleanup
        if (m_vulkanSurface)
        {
            m_vulkanSurface->destroy();
        }

        if (m_vulkanInstance)
        {
            m_vulkanInstance->destroy();
        }
    }
}   // namespace keplar
