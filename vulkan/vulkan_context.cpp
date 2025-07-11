// ────────────────────────────────────────────
//  File: vulkan_context.cpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#include "vulkan_context.hpp"
#include "utils/logger.hpp"
#include "vk_utils.hpp"

namespace keplar
{
    VulkanContext::VulkanContext()
        : m_vulkanInstance(std::make_unique<VulkanInstance>())
        , m_vulkanSurface(std::make_unique<VulkanSurface>())
        , m_vulkanDevice(std::make_unique<VulkanDevice>())
    {
    }

    VulkanContext::~VulkanContext()
    {
        // TODO: handle based on m_isDestroyed to avoid redundant checks
        destroy();
    }

    bool VulkanContext::initialize(const Platform& platform, const VulkanContextConfig& config)
    {
        // create vulkan instance 
        if (!m_vulkanInstance->initialize(config))
        {
            return false;
        }

        // create presentation surface 
        if (!m_vulkanSurface->initialize(*m_vulkanInstance, platform))
        {
            return false;
        }

        // choose appropriate physical device and create logical device with requested queues, features and extensions
        if (!m_vulkanDevice->initialize(*m_vulkanInstance, *m_vulkanSurface, config))
        {
            return false;
        }

        return true;
    }

    void VulkanContext::destroy()
    {
        // destroy in reverse order of creation for safe cleanup
        if (m_vulkanDevice)
        {
            m_vulkanDevice->destroy();
        }

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

    VulkanContext::Builder& VulkanContext::Builder::enableValidation(bool enable)
    {
        m_vulkanContextConfig.mEnableValidation = enable;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withPlatform(const Platform& platform)
    {
        m_platform = &platform;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withInstanceExtensions(const std::vector<const char*>& instanceExtensions)
    {
        m_vulkanContextConfig.mInstanceExtensions = instanceExtensions;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withValidationLayers(const std::vector<const char*>& validationLayers)
    {
        m_vulkanContextConfig.mValidationLayers = validationLayers;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withDeviceExtensions(const std::vector<const char*>& deviceExtensions)
    {
        m_vulkanContextConfig.mDeviceExtensions = deviceExtensions;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withDeviceFeatures(VkPhysicalDeviceFeatures features)
    {
        m_vulkanContextConfig.mRequestedFeatures = features;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::preferDedicatedComputeQueue()
    {
        m_vulkanContextConfig.mPreferDedicatedComputeQueue = true;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::preferDedicatedTransferQueue()
    {
        m_vulkanContextConfig.mPreferDedicatedTransferQueue = true;
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
            m_vulkanContextConfig.mInstanceExtensions.emplace_back(extension);
        }

        // add default validation layer if not provided
        if (m_vulkanContextConfig.mEnableValidation && m_vulkanContextConfig.mValidationLayers.empty())
        {
            // VK_EXT_debug_utils extension is must required to receive detailed debug messages from validation layers
            m_vulkanContextConfig.mInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
