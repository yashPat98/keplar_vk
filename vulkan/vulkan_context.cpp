// ────────────────────────────────────────────
//  File: vulkan_context.cpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#include "vulkan_context.hpp"
#include "utils/logger.hpp"
#include "vk_utils.hpp"

namespace keplar
{
    VulkanContext::VulkanContext()
        : m_vulkanInstance(nullptr)
        , m_vulkanSurface(nullptr)
    {
    }

    VulkanContext::~VulkanContext()
    {
        // TODO: handle based on m_isDestroyed to avoid redundant checks
        destroy();
    }

    bool VulkanContext::initialize(const Platform& platform, const VulkanContextConfig& config)
    {
        // construct vulkan subsystem wrapper objects
        m_vulkanInstance = std::make_unique<VulkanInstance>();
        m_vulkanSurface = std::make_unique<VulkanSurface>();

        // create vulkan instance based on config
        if (!m_vulkanInstance->initialize(config))
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

    // ─────────────── VulkanContext::Builder ───────────────

    VulkanContext::Builder& VulkanContext::Builder::withPlatform(const Platform& platform)
    {
        m_platform = &platform;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withInstanceExtensions(const std::vector<const char*>& extensions)
    {
        m_vulkanContextConfig.mExtensions = extensions;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withValidationLayers(const std::vector<const char*>& validationLayers)
    {
        m_vulkanContextConfig.mValidationLayers = validationLayers;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::enableValidation(bool enable)
    {
        m_vulkanContextConfig.mEnableValidation = enable;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withApplicationName(std::string_view appName)
    {
        m_vulkanContextConfig.mApplicationName = appName;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withApplicationVersion(uint32_t appVersion)
    {
        m_vulkanContextConfig.mApplicationVersion = appVersion;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withEngineName(std::string_view engineName)
    {
        m_vulkanContextConfig.mEngineName = engineName;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withEngineVersion(uint32_t engineVersion)
    {
        m_vulkanContextConfig.mEngineVersion = engineVersion;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withApiVersion(uint32_t apiVersion)
    {
        m_vulkanContextConfig.mApiVersion = apiVersion;
        return *this;
    }

    std::unique_ptr<VulkanContext> VulkanContext::Builder::build()
    {
        // ensure a valid platform is set 
        if (!m_platform)
        {
            VK_LOG_FATAL("VulkanContext::Builder::build :: Platform not set.");
            return nullptr;
        }

        // add platform-specific extensions to config
        for (const char* extension : m_platform->getSurfaceInstanceExtensions())
        {
            m_vulkanContextConfig.mExtensions.emplace_back(extension);
        }

        // add default validation layer if not provided
        if (m_vulkanContextConfig.mEnableValidation && m_vulkanContextConfig.mValidationLayers.empty())
        {
            // VK_EXT_debug_utils extension is must required to receive detailed debug messages from validation layers
            m_vulkanContextConfig.mExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            m_vulkanContextConfig.mValidationLayers.emplace_back(VK_LAYER_KHRONOS_VALIDATION_NAME);
        }

        // create and initialize VulkanContext
        auto vulkanContext = std::unique_ptr<VulkanContext>(new VulkanContext());
        if (!vulkanContext->initialize(*m_platform, m_vulkanContextConfig))
        {
            VK_LOG_FATAL("VulkanContext::Builder::build :: failed to build VulkanContext");
            return nullptr;
        }

        return vulkanContext;
    }

}   // namespace keplar
