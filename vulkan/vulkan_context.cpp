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

        return true;
    }

    // ─────────────── VulkanContext::Builder ───────────────

    VulkanContext::Builder& VulkanContext::Builder::withPlatform(const Platform& platform) noexcept
    {
        m_platform = &platform;
        return *this;
    }

    VulkanContext::Builder& VulkanContext::Builder::withConfig(const VulkanContextConfig& config) noexcept
    {
        m_config = config;
        return *this;
    }

    std::unique_ptr<VulkanContext> VulkanContext::Builder::build()
    {
        // context requires a valid platform
        if (!m_platform)
        {
            VK_LOG_FATAL("VulkanContext::Builder::build failed: platform is not specified");
            return nullptr;
        }

         // context require explicit vulkan config
        if (!m_config.has_value())
        {
            VK_LOG_FATAL("VulkanContext::Builder::build failed: context config is not specified");
            return nullptr;
        }

        // append platform-specific instance extensions 
        VulkanContextConfig config = *m_config;
        const auto& platformExtensions = m_platform->getSurfaceInstanceExtensions();
        config.mInstanceExtensions.insert(config.mInstanceExtensions.begin(), platformExtensions.begin(), platformExtensions.end());

        // set default validation layer if none provided
        if (config.mEnableValidation && config.mValidationLayers.empty())
        {
            // VK_EXT_debug_utils extension is must required to receive detailed debug messages from validation layers
            config.mInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            config.mValidationLayers.emplace_back(VK_LAYER_KHRONOS_VALIDATION_NAME);
        }

        // create and initialize context
        auto vulkanContext = std::unique_ptr<VulkanContext>(new VulkanContext);
        if (!vulkanContext->initialize(*m_platform, config))
        {
            VK_LOG_FATAL("VulkanContext::Builder::build : failed to build VulkanContext");
            return nullptr;
        }

        return vulkanContext;
    }

}   // namespace keplar
