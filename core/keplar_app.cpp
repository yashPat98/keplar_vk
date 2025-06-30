// ────────────────────────────────────────────
//  File: keplar_app.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "keplar_app.hpp"
#include "utils/logger.hpp"
#include "platform/platform_factory.hpp"

namespace keplar
{
    constexpr const char* kWindowTitle = "keplar_vk";
    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 720;
}

namespace keplar
{
    KeplarApp::KeplarApp()
        : m_vulkanContext(std::make_unique<VulkanContext>())
    {
    }

    bool KeplarApp::initialize()
    {
        // create a platform-specific implementation using a factory function.
        m_platform = platform::createPlatform();
        if (!m_platform)
        {
            VK_LOG_FATAL("failed to create platform instance");
            return false;
        }

        // initialize the platform window and system resources
        if (!m_platform->initialize(kWindowTitle, kDefaultWidth, kDefaultHeight))
        {
            return false;
        }

        // set up required vulkan instance extensions and validation layers 
        VulkanContextConfig vulkanContextConfig {};
        vulkanContextConfig.mExtensions = m_platform->getSurfaceInstanceExtensions();
    
        if (kEnableValidationLayers) 
        {
            // add debug extension and validation layers
            vulkanContextConfig.mExtensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            vulkanContextConfig.mValidationLayers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

        // create and initialize vulkan context with above config 
        m_vulkanContext = std::make_unique<VulkanContext>();
        if (!m_vulkanContext->initialize(*m_platform, vulkanContextConfig))
        {
            return false;
        }

        return true;
    }

    bool KeplarApp::run()
    {
        while (!m_platform->shouldClose())
        {
            m_platform->pollEvents();
        }
        return true;
    }

    void KeplarApp::shutdown()
    {
        // destroy in reverse order for safe cleanup
        if (m_vulkanContext)
        {
            m_vulkanContext->destroy();
        }

        if (m_platform)
        {
            m_platform->shutdown();
        }
    }

}   // namespace keplar