// ────────────────────────────────────────────
//  File: keplar_app.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "keplar_app.hpp"
#include "utils/logger.hpp"
#include "platform/platform_factory.hpp"
#include "vulkan/vk_utils.hpp"

namespace keplar
{
    inline constexpr const char* kWindowTitle = "keplar_vk";
    inline constexpr int kDefaultWidth = 1280;
    inline constexpr int kDefaultHeight = 720;
}

namespace keplar
{
    KeplarApp::KeplarApp()
        : m_platform(nullptr)
        , m_vulkanContext(nullptr)
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

        // create vulkan context using the builder pattern 
        m_vulkanContext = VulkanContext::Builder::Builder()
                            .withPlatform(*m_platform)
                            .withInstanceExtensions({ VK_EXT_DEBUG_UTILS_EXTENSION_NAME })
                            .withValidationLayers({ VK_LAYER_KHRONOS_VALIDATION_NAME })
                            .withDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME })
                            .build();
                            
        if (!m_vulkanContext)
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