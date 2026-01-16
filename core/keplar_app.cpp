// ────────────────────────────────────────────
//  File: keplar_app.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "keplar_app.hpp"

#include <chrono> 

#include "keplar_config.hpp"
#include "platform/platform.hpp"
#include "platform/platform_factory.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "graphics/renderer.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    KeplarApp::KeplarApp() noexcept
        : m_platform(nullptr)
        , m_vulkanContext(nullptr)
        , m_renderer(nullptr)
    {
        // set frame rate
        m_framePacer.setTargetFps(keplar::config::kDefaultFrameRate);
    }

    KeplarApp::~KeplarApp()
    {
        shutdown();
    }

    bool KeplarApp::initialize(std::unique_ptr<Renderer> renderer) noexcept
    {
        // acquire renderer
        m_renderer = std::move(renderer);
        if (!m_renderer) 
        {
            VK_LOG_ERROR("KeplarApp::initialize failed: renderer is null");
            return false;
        }

        // high-level orchestration of subsystem creation
        if (!initializePlatform())   { return false; }
        if (!initializeContext())    { return false; }
        if (!initializeRenderer())   { return false; }

        VK_LOG_INFO("KeplarApp::initialize successful");
        return true;
    }

    int KeplarApp::run() noexcept
    {
        while (!m_platform->shouldClose())
        {
            // process platform events
            m_platform->pollEvents();

            // update state and submit the current frame
            m_renderer->update(m_time.tick());
            if (!m_renderer->render()) 
                return EXIT_FAILURE; 
            
            // frame pacing
            m_framePacer.wait();
        }

        return EXIT_SUCCESS;
    }

    void KeplarApp::shutdown() noexcept
    {
        // teardown subsystems in reverse order
        m_platform->removeListener(m_renderer);
        m_renderer.reset();
        m_vulkanContext.reset();
        m_platform.reset();
    }

    bool KeplarApp::initializePlatform() noexcept
    {
        // create platform-specific implementation
        m_platform = platform::createPlatform();
        if (!m_platform)
        {
            VK_LOG_FATAL("Failed to create platform instance");
            return false;
        }

        // initialize the platform window and system resources
        if (!m_platform->initialize(config::kWindowTitle, config::kDefaultWidth, config::kDefaultHeight, config::kStartMaximized))
        {
            VK_LOG_DEBUG("Failed to initialize platform window");
            return false;
        }

        return true;
    }

    bool KeplarApp::initializeContext() noexcept
    {
        // prepare vulkan configuration
        VulkanContextConfig config{};
        config.mDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        // append validation extensions and layers if enabled
        if (config.mEnableValidation)
        {
            config.mInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            config.mValidationLayers.emplace_back(VK_LAYER_KHRONOS_VALIDATION_NAME);
        }

        // allow renderer to configure vulkan context
        m_renderer->configureVulkan(config);

        // build vulkan context using platform and config
        m_vulkanContext = VulkanContext::Builder()
                            .withPlatform(m_platform)
                            .withConfig(config)
                            .build();
        if (!m_vulkanContext)
        {
            VK_LOG_DEBUG("Failed to initialize vulkan context");
            return false;
        }

        return true;
    }

    bool KeplarApp::initializeRenderer() noexcept
    {
        // create and initialize renderer
        if (!m_renderer->initialize(m_platform, m_vulkanContext))
        {
            VK_LOG_DEBUG("Failed to initialize renderer");
            return false;
        }

        // register renderer as event listener
        m_platform->addListener(m_renderer);
        return true;
    }

}   // namespace keplar