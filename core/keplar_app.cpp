// ────────────────────────────────────────────
//  File: keplar_app.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "keplar_app.hpp"
#include "config.hpp"
#include "utils/logger.hpp"
#include "platform/platform_factory.hpp"
#include "vulkan/vk_utils.hpp"

namespace keplar
{
    KeplarApp::KeplarApp() noexcept
        : m_platform(nullptr)
        , m_vulkanContext(nullptr)
        , m_renderer(nullptr)
    {
    }

    KeplarApp::~KeplarApp()
    {
        shutdown();
    }

    bool KeplarApp::initialize() noexcept
    {
        // high-level orchestration of subsystem creation
        if (!createPlatform())   { return false; }
        if (!createContext())    { return false; }
        if (!createRenderer())   { return false; }

        VK_LOG_INFO("KeplarApp::initialize successful");
        return true;
    }

    int KeplarApp::run() noexcept
    {
        while (!m_platform->shouldClose())
        {
            m_platform->pollEvents();
            m_renderer->renderFrame();
        }
        return EXIT_SUCCESS;
    }

    void KeplarApp::shutdown() noexcept
    {
        // teardown subsystems in reverse order
        m_platform->removeListener(m_renderer.get());
        m_renderer.reset();
        m_vulkanContext.reset();
        m_platform.reset();
    }

    bool KeplarApp::createPlatform() noexcept
    {
        // create platform-specific implementation
        m_platform = platform::createPlatform();
        if (!m_platform)
        {
            VK_LOG_FATAL("Failed to create platform instance");
            return false;
        }

        // initialize the platform window and system resources
        if (!m_platform->initialize(config::kWindowTitle, config::kDefaultWidth, config::kDefaultHeight))
        {
            VK_LOG_DEBUG("Failed to initialize platform window");
            return false;
        }

        return true;
    }

    bool KeplarApp::createContext() noexcept
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

        // build vulkan context using platform and config
        m_vulkanContext = VulkanContext::Builder()
                            .withPlatform(*m_platform)
                            .withConfig(config)
                            .build();
        if (!m_vulkanContext)
        {
            VK_LOG_DEBUG("Failed to create vulkan context");
            return false;
        }

        return true;
    }

    bool KeplarApp::createRenderer() noexcept
    {
        // query current window dimensions
        uint32_t width  = m_platform->getWindowWidth();
        uint32_t height = m_platform->getWindowHeight();

        // create and initialize renderer
        m_renderer = std::make_unique<Renderer>(*m_vulkanContext, width, height);
        if (!m_renderer->initialize())
        {
            VK_LOG_DEBUG("Failed to initialize renderer");
            return false;
        }

        // register renderer as event listener
        m_platform->addListener(m_renderer.get());

        return true;
    }

}   // namespace keplar