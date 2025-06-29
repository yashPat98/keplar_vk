// ────────────────────────────────────────────
//  File: vulkan_instance.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "vulkan_instance.hpp"

#include <algorithm> 
#include "utils/logger.hpp"

namespace keplar
{
    VulkanInstance::VulkanInstance()
        : m_vkInstance(VK_NULL_HANDLE)
    {
    }

    VulkanInstance::~VulkanInstance()
    {
        destroy();
    }

    bool VulkanInstance::initialize(const std::vector<const char*>& extensions, const std::vector<const char*>& validationLayers)
    {
        // set extensions to be enabled
        if (!validateAndSetExtensions(extensions))
        {
            VK_LOG_ERROR("validateAndSetExtensions failed");
            return false;
        }

        // set validation layers to be enabled
        if constexpr (kEnableValidationLayers)
        {
            if (!validateAndSetValidationLayers(validationLayers))
            {
                VK_LOG_ERROR("validateAndSetValidationLayers failed");
                return false;
            }
        }

        // set up vulkan application info
        VkApplicationInfo vkApplicationInfo {};
        vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vkApplicationInfo.pNext = nullptr;
        vkApplicationInfo.pApplicationName = kApplicationName;
        vkApplicationInfo.applicationVersion = kApplicationVersion;
        vkApplicationInfo.pEngineName = kEngineName;
        vkApplicationInfo.engineVersion = kEngineVersion;
        vkApplicationInfo.apiVersion = kVulkanApiVersion;

        // set up vulkan instance create info
        VkInstanceCreateInfo vkInstanceCreateInfo {};
        vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkInstanceCreateInfo.pNext = nullptr;
        vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
        vkInstanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_enabledExtensions.size());
        vkInstanceCreateInfo.ppEnabledExtensionNames = m_enabledExtensions.data();

        // set validation layers if enabled
        if constexpr (kEnableValidationLayers)
        {
            vkInstanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_enabledValidationLayers.size());
            vkInstanceCreateInfo.ppEnabledLayerNames = m_enabledValidationLayers.data();
        }

        // create the vulkan instance
        VkResult vkResult = vkCreateInstance(&vkInstanceCreateInfo, nullptr, &m_vkInstance);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateInstance failed to create vulkan instance : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        VK_LOG_INFO("vulkan instance created successfully");
        return true;
    }

    void VulkanInstance::destroy()
    {
        // destroy vulkan instance after all vulkan resources have been released
        if (m_vkInstance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_vkInstance, nullptr);
            m_vkInstance = VK_NULL_HANDLE;
            VK_LOG_INFO("vulkan instance destroyed successfully");
        }

        // clear the enabled extensions and validation layers
        m_enabledExtensions.clear();
        m_enabledValidationLayers.clear();
    }

    VkInstance VulkanInstance::get() const
    {
        return m_vkInstance;
    }

    bool VulkanInstance::isValid() const
    {
        return (m_vkInstance != VK_NULL_HANDLE);
    }

    bool VulkanInstance::isExtensionEnabled(const char* extensionName) const
    {
        // check if the extension is enabled
        return std::find(m_enabledExtensions.begin(), m_enabledExtensions.end(), extensionName) != m_enabledExtensions.end();
    }

    bool VulkanInstance::isValidationLayerEnabled(const char* layerName) const
    {
        // check if the validation layer is enabled
        return std::find(m_enabledValidationLayers.begin(), m_enabledValidationLayers.end(), layerName) != m_enabledValidationLayers.end();
    }

    bool VulkanInstance::validateAndSetExtensions(const std::vector<const char*>& requestedExtensions)
    {
        // vulkan spec allows instance creation without any extensions
        if (requestedExtensions.empty())
        {
            VK_LOG_WARN("no instance extensions requested");
            return true;
        }

        // get the count of supported instance extensions
        uint32_t extensionCount = 0;
        VkResult vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance extension count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // get the supported instance extensions
        std::vector<VkExtensionProperties> vkExtensionProperties(extensionCount);
        vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, vkExtensionProperties.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance extension properties : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // create a set of supported extensions for fast lookup
        std::unordered_set<std::string> supportedExtensions;
        supportedExtensions.reserve(extensionCount);
        for (const auto& elm : vkExtensionProperties)
        {
            supportedExtensions.insert(elm.extensionName);
        }

        // log the supported extensions in debug mode
        if (Logger::getInstance().isEnabled(Logger::Level::Debug))
        {
            for (const auto& extension : supportedExtensions)
            {
                VK_LOG_DEBUG("supported instance extension: %s", extension.c_str());
            }
        }

        // check if the requested extensions are supported 
        m_enabledExtensions.reserve(requestedExtensions.size());
        std::unordered_set<std::string> seen;
        for (const auto& extension : requestedExtensions)
        {
            // skip duplicates 
            if (!extension || *extension == '\0' || !seen.insert(extension).second)
            {
                continue;
            }

            // check if the extension is supported
            if (supportedExtensions.find(extension) == supportedExtensions.end())
            {
                VK_LOG_FATAL("requested instance extension %s is not supported by vulkan driver", extension);
                return false;
            }
            
            // add the extension to the enabled extensions
            VK_LOG_INFO("enabled instance extension: %s", extension);
            m_enabledExtensions.emplace_back(extension);
        }

        return true;
    }

    bool VulkanInstance::validateAndSetValidationLayers(const std::vector<const char*>& requestedLayers) 
    {
        // validation layers are enabled but no layers are requested 
        if (requestedLayers.empty())
        {
            VK_LOG_WARN("no instance validation layers requested");
            return true;
        }

        // get the count of supported validation layers
        uint32_t layerCount = 0;
        VkResult vkResult = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance validation layer count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }
 
        // get the supported validation layers
        std::vector<VkLayerProperties> vkLayerProperties(layerCount);
        vkResult = vkEnumerateInstanceLayerProperties(&layerCount, vkLayerProperties.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance validation layer properties : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // create a set of supported layers for fast lookup
        std::unordered_set<std::string> supportedLayers;
        supportedLayers.reserve(layerCount);
        for (const auto& layer : vkLayerProperties)
        {
            supportedLayers.insert(layer.layerName);
        }

        // log the supported layers in debug mode
        if (Logger::getInstance().isEnabled(Logger::Level::Debug))
        {
            for (const auto& layer : supportedLayers)
            {
                VK_LOG_DEBUG("supported instance validation layer: %s", layer.c_str());
            }
        }

        // check if the requested layers are supported
        m_enabledValidationLayers.reserve(requestedLayers.size());
        std::unordered_set<std::string> seen;
        for (const auto& layer : requestedLayers)
        {
            // skip duplicates
            if (!layer || *layer == '\0' || !seen.insert(layer).second)
            {
                continue;
            }

            // check if the layer is supported
            if (supportedLayers.find(layer) == supportedLayers.end())
            {
                VK_LOG_FATAL("requested instance validation layer %s is not supported by vulkan driver", layer);
                return false;
            }

            // add the layer to the enabled validation layers
            m_enabledValidationLayers.emplace_back(layer);
            VK_LOG_INFO("enabled instance validation layer: %s", layer);
        }

        return true;
    }
}   // namespace keplar
