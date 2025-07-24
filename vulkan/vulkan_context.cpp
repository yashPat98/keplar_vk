// ────────────────────────────────────────────
//  File: vulkan_context.cpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#include "vulkan_context.hpp"
#include "utils/logger.hpp"
#include "vk_utils.hpp"

namespace keplar
{
    VulkanContext::VulkanContext() noexcept
    {
    }

    VulkanContext::~VulkanContext()
    {
        // destroy resources in a reverse order
        m_vulkanSwapchain.reset();
        m_vulkanDevice.reset();
        m_vulkanSurface.reset();
        m_vulkanInstance.reset();
    }

    bool VulkanContext::initialize(const Platform& platform, const VulkanContextConfig& config) noexcept
    {
        // create vulkan instance 
        m_vulkanInstance = VulkanInstance::create(config);
        if (!m_vulkanInstance)
        {
            return false;
        }

        // create presentation surface 
        m_vulkanSurface = VulkanSurface::create(*m_vulkanInstance, platform);
        if (!m_vulkanSurface)
        {
            return false;
        }

        // choose appropriate physical device and create logical device 
        m_vulkanDevice = VulkanDevice::create(*m_vulkanInstance, *m_vulkanSurface, config);
        if (!m_vulkanDevice)
        {
            return false;
        }

        // create swapchain and image views
        VkExtent2D windowExtent{ platform.getWindowWidth(), platform.getWindowHeight() };
        m_vulkanSwapchain = VulkanSwapchain::create(*m_vulkanSurface, *m_vulkanDevice, windowExtent);
        if (!m_vulkanSwapchain)
        {
            return false;
        }

        return true;
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

    VulkanContext::Builder& VulkanContext::Builder::withInstanceExtensions(const std::vector<std::string_view>& instanceExtensions)
    {
        auto& existingExtensions = m_vulkanContextConfig.mInstanceExtensions;
        existingExtensions.reserve(existingExtensions.size() + instanceExtensions.size());

        std::unordered_set<std::string_view> seen(existingExtensions.begin(), existingExtensions.end());
        for (const auto& extension : instanceExtensions)
        {
            // skip duplicates and empty strings
            if (extension.empty() || !seen.insert(extension).second) 
            {
                continue;
            }
            existingExtensions.emplace_back(extension);
        }
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withInstanceExtensionsIfValidation(const std::vector<std::string_view>& instanceExtensions)
    {
        if (!m_vulkanContextConfig.mEnableValidation)
        {
            return *this;
        }
        
        auto& existingExtensions = m_vulkanContextConfig.mInstanceExtensions;
        existingExtensions.reserve(existingExtensions.size() + instanceExtensions.size());

        std::unordered_set<std::string_view> seen(existingExtensions.begin(), existingExtensions.end());
        for (const auto& extension : instanceExtensions)
        {
            // skip duplicates and empty strings
            if (extension.empty() || !seen.insert(extension).second) 
            {
                continue;
            }
            existingExtensions.emplace_back(extension);
        }
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withValidationLayers(const std::vector<std::string_view>& validationLayers)
    {
        auto& existingLayers = m_vulkanContextConfig.mValidationLayers;
        existingLayers.reserve(existingLayers.size() + validationLayers.size());

        std::unordered_set<std::string_view> seen(existingLayers.begin(), existingLayers.end());
        for (auto layer : validationLayers)
        {
            if (layer.empty() || !seen.insert(layer).second)
            {
                continue;
            }
            existingLayers.emplace_back(layer);
        }
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withDeviceExtensions(const std::vector<std::string_view>& deviceExtensions)
    {
        auto& existingExtensions = m_vulkanContextConfig.mDeviceExtensions;
        existingExtensions.reserve(existingExtensions.size() + deviceExtensions.size());

        std::unordered_set<std::string_view> seen(existingExtensions.begin(), existingExtensions.end());
        for (auto extension : deviceExtensions)
        {
            if (extension.empty() || !seen.insert(extension).second)
            {
                continue;
            }
            existingExtensions.emplace_back(extension);
        }
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
        const auto& platformExtensions = m_platform->getSurfaceInstanceExtensions();
        auto& instanceExtensions = m_vulkanContextConfig.mInstanceExtensions;
        instanceExtensions.insert(instanceExtensions.begin(), platformExtensions.begin(), platformExtensions.end());

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
