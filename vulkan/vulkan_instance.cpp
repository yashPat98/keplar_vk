// ────────────────────────────────────────────
//  File: vulkan_instance.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "vulkan_instance.hpp"

#include <sstream>
#include <algorithm> 

#include "vulkan_utils.hpp"
#include "utils/logger.hpp"

namespace 
{
    // vulkan debug utils function name strings for dynamic loading
    constexpr const char* kCreateDebugUtilsMessenger  = "vkCreateDebugUtilsMessengerEXT";
    constexpr const char* kDestroyDebugUtilsMessenger = "vkDestroyDebugUtilsMessengerEXT";
}

namespace keplar
{
    std::shared_ptr<VulkanInstance> VulkanInstance::create(const VulkanContextConfig& config) noexcept
    {
        std::shared_ptr<VulkanInstance> instance(new VulkanInstance);
        if (!instance->initialize(config))
        {
            return nullptr;
        }
        return instance;
    }

    VulkanInstance::VulkanInstance() noexcept
        : m_vkInstance(VK_NULL_HANDLE)
        , m_vkDebugUtilsMessengerEXT(VK_NULL_HANDLE)
    {
    }

    VulkanInstance::~VulkanInstance()
    {
        // destroy debug utils messenger
        if (m_vkDebugUtilsMessengerEXT != VK_NULL_HANDLE)
        {
            // retrieve pfn for destroying debug messenger
            auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, kDestroyDebugUtilsMessenger);
            if (vkDestroyDebugUtilsMessengerEXT)
            {
                vkDestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugUtilsMessengerEXT, nullptr);
                m_vkDebugUtilsMessengerEXT = VK_NULL_HANDLE;
                VK_LOG_INFO("vulkan debug utils messenger destroyed successfully");
            }
            else 
            {
                VK_LOG_ERROR("vkGetInstanceProcAddr failed to get vkDestroyDebugUtilsMessengerEXT function pointer");
            }
        }

        // destroy vulkan instance after all vulkan resources have been released
        if (m_vkInstance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_vkInstance, nullptr);
            m_vkInstance = VK_NULL_HANDLE;
            VK_LOG_INFO("vulkan instance destroyed successfully");
        }
    }

    VkInstance VulkanInstance::get() const noexcept
    {
        return m_vkInstance;
    }

    bool VulkanInstance::isValid() const noexcept
    {
        return (m_vkInstance != VK_NULL_HANDLE);
    }

    bool VulkanInstance::isExtensionEnabled(const char* extensionName) const noexcept
    {
        // check if the extension is enabled
        return std::find(m_enabledExtensions.begin(), m_enabledExtensions.end(), extensionName) != m_enabledExtensions.end();
    }

    bool VulkanInstance::isValidationLayerEnabled(const char* layerName) const noexcept
    {
        // check if the validation layer is enabled
        return std::find(m_enabledValidationLayers.begin(), m_enabledValidationLayers.end(), layerName) != m_enabledValidationLayers.end();
    }

    bool VulkanInstance::initialize(const VulkanContextConfig& config) noexcept
    {
        // set extensions to be enabled
        if (!validateAndSetExtensions(config.mInstanceExtensions))
        {
            VK_LOG_ERROR("validateAndSetExtensions failed");
            return false;
        }

        // set validation layers to be enabled
        if (config.mEnableValidation && !validateAndSetValidationLayers(config.mValidationLayers))
        {
            VK_LOG_ERROR("validateAndSetValidationLayers failed");
            return false;
        }

        // set up vulkan application info
        VkApplicationInfo vkApplicationInfo {};
        vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vkApplicationInfo.pNext = nullptr;
        vkApplicationInfo.pApplicationName = config.mApplicationName.data();
        vkApplicationInfo.applicationVersion = config.mApplicationVersion;
        vkApplicationInfo.pEngineName = config.mEngineName.data();
        vkApplicationInfo.engineVersion = config.mEngineVersion;
        vkApplicationInfo.apiVersion = config.mApiVersion;

        // set up vulkan instance create info
        VkInstanceCreateInfo vkInstanceCreateInfo {};
        vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkInstanceCreateInfo.pNext = nullptr;
        vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
        vkInstanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_enabledExtensions.size());
        vkInstanceCreateInfo.ppEnabledExtensionNames = m_enabledExtensions.data();

        // setup validation info
        VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfoEXT{};
        if (config.mEnableValidation)
        {
            // set validation layers
            vkInstanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_enabledValidationLayers.size());
            vkInstanceCreateInfo.ppEnabledLayerNames = m_enabledValidationLayers.data();

            // setup debug messenger creation info 
            vkDebugUtilsMessengerCreateInfoEXT.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            vkDebugUtilsMessengerCreateInfoEXT.pNext = nullptr;
            vkDebugUtilsMessengerCreateInfoEXT.flags = 0;
            vkDebugUtilsMessengerCreateInfoEXT.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            vkDebugUtilsMessengerCreateInfoEXT.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            vkDebugUtilsMessengerCreateInfoEXT.pfnUserCallback = VulkanInstance::debugUtilsMessageCallback;

            // set debug utils messengers create info to catch validations during instance creation and destruction
            vkInstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&vkDebugUtilsMessengerCreateInfoEXT;
        }

        // create the vulkan instance
        VkResult vkResult = vkCreateInstance(&vkInstanceCreateInfo, nullptr, &m_vkInstance);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateInstance failed to create vulkan instance : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }
        else 
        {
            VK_LOG_INFO("vulkan instance created successfully");
        }

        // setup debug messenger 
        if (config.mEnableValidation && !setupDebugMessenger())
        {
            VK_LOG_ERROR("setupDebugMessenger failed");
            return false;
        }
        
        return true;
    }

    bool VulkanInstance::setupDebugMessenger() noexcept
    {
        // retrieve pfn for creating debug messenger
        auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, kCreateDebugUtilsMessenger);
        if (!vkCreateDebugUtilsMessengerEXT)
        {
             VK_LOG_ERROR("vkGetInstanceProcAddr failed to get vkCreateDebugUtilsMessengerEXT function pointer");
            return false;
        }

        // setup debug messenger creation info 
        VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfoEXT{};
        vkDebugUtilsMessengerCreateInfoEXT.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        vkDebugUtilsMessengerCreateInfoEXT.pNext = nullptr;
        vkDebugUtilsMessengerCreateInfoEXT.flags = 0;
        vkDebugUtilsMessengerCreateInfoEXT.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        vkDebugUtilsMessengerCreateInfoEXT.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        vkDebugUtilsMessengerCreateInfoEXT.pfnUserCallback = VulkanInstance::debugUtilsMessageCallback;
  
        // create the debug messenger
        VkResult vkResult = vkCreateDebugUtilsMessengerEXT(m_vkInstance, &vkDebugUtilsMessengerCreateInfoEXT, nullptr, &m_vkDebugUtilsMessengerEXT);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateDebugUtilsMessengerEXT failed to create debug utils messenger : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }
        
        VK_LOG_INFO("vulkan debug utils messenger created successfully");
        return true;
    }

    bool VulkanInstance::validateAndSetExtensions(const std::vector<std::string_view>& requestedExtensions) noexcept
    {
        // vulkan spec allows instance creation without any extensions
        if (requestedExtensions.empty())
        {
            VK_LOG_WARN("no instance extensions requested");
            return true;
        }

        // query the count of supported instance extensions
        uint32_t extensionCount = 0;
        VkResult vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance extension count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // retrieve the supported instance extensions
        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance extension properties : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // create a set of supported extensions for fast lookup
        std::unordered_set<std::string_view> supportedExtensions;
        supportedExtensions.reserve(extensionCount);
        for (const auto& elm : extensionProperties)
        {
            supportedExtensions.insert(elm.extensionName);
            VK_LOG_DEBUG("supported instance extension: %s", elm.extensionName);
        }

        // check if the requested extensions are supported 
        m_enabledExtensions.reserve(requestedExtensions.size());
        for (const auto& extension : requestedExtensions)
        {
            if (supportedExtensions.find(extension) == supportedExtensions.end())
            {
                VK_LOG_FATAL("requested instance extension %s is not supported by vulkan driver", extension.data());
                return false;
            }
            
            // add the extension to the enabled extensions
            VK_LOG_INFO("enabled instance extension: %s", extension.data());
            m_enabledExtensions.emplace_back(extension.data());
        }

        return true;
    }

    bool VulkanInstance::validateAndSetValidationLayers(const std::vector<std::string_view>& requestedLayers) noexcept
    {
        // validation layers are enabled but no layers are requested 
        if (requestedLayers.empty())
        {
            VK_LOG_WARN("no instance validation layers requested");
            return true;
        }

        // query the count of supported validation layers
        uint32_t layerCount = 0;
        VkResult vkResult = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance validation layer count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }
 
        // retrieve the supported validation layers
        std::vector<VkLayerProperties> layerProperties(layerCount);
        vkResult = vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query instance validation layer properties : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // create a set of supported layers for fast lookup
        std::unordered_set<std::string_view> supportedLayers;
        supportedLayers.reserve(layerCount);
        for (const auto& layer : layerProperties)
        {
            supportedLayers.insert(layer.layerName);
            VK_LOG_DEBUG("supported instance validation layer: %s", layer.layerName);
        }

        // check if the requested layers are supported
        m_enabledValidationLayers.reserve(requestedLayers.size());
        for (const auto& layer : requestedLayers)
        {
            if (supportedLayers.find(layer) == supportedLayers.end())
            {
                VK_LOG_FATAL("requested instance validation layer %s is not supported by vulkan driver", layer.data());
                return false;
            }

            // add the layer to the enabled validation layers
            m_enabledValidationLayers.emplace_back(layer.data());
            VK_LOG_INFO("enabled instance validation layer: %s", layer.data());
        }

        return true;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanInstance::debugUtilsMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
                                                                             VkDebugUtilsMessageTypeFlagsEXT messageType, 
                                                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
                                                                             void* /*pUserData*/)
    {
        // vulkan spec guarantees that only one severity bit is set per message
        Logger::Level logLevel; 
        switch (messageSeverity) 
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:   logLevel = Logger::Level::Debug; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:      logLevel = Logger::Level::Info; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:   logLevel = Logger::Level::Warn; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:     logLevel = Logger::Level::Error; break;
            default:                                                logLevel = Logger::Level::Info; break;
        }

        // construct message type string from bitflags
        std::string messageTypeStr;
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)      { messageTypeStr += "GENERAL "; }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)   { messageTypeStr += "VALIDATION "; }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)  { messageTypeStr += "PERFORMANCE "; }
        if (!messageTypeStr.empty() && messageTypeStr.back() == ' ')        { messageTypeStr.pop_back(); }

        // retrieve message ID and name safely
        int32_t messageId = pCallbackData ? pCallbackData->messageIdNumber : -1;
        const char* messageIdName = (pCallbackData && pCallbackData->pMessageIdName) ? pCallbackData->pMessageIdName : "unknown";
        
        // extract and split full message into main and spec parts
        std::string_view fullMessage = (pCallbackData && pCallbackData->pMessage) ? pCallbackData->pMessage : "no message retrieved";
        constexpr std::string_view specHint = "The Vulkan spec states:";
        std::string_view messageOnly;
        std::string_view specOnly;

        if (size_t specPos = fullMessage.find(specHint); specPos != std::string_view::npos)
        {
            messageOnly = fullMessage.substr(0, specPos);
            specOnly = fullMessage.substr(specPos);
        }
        else
        {
            messageOnly = fullMessage;
        }

        // trim leading "| vk" prefix from messageOnly if present
        constexpr std::string_view vkCallPrefix = "| vk";
        size_t vkCallStart = messageOnly.find(vkCallPrefix);
        if (vkCallStart != std::string_view::npos)
        {
            messageOnly = messageOnly.substr(vkCallStart + 2); // skip "| "
            messageOnly.remove_prefix(messageOnly.find_first_not_of(' '));
        }

        // trim trailing newlines from messageOnly
        while (!messageOnly.empty() && (messageOnly.back() == '\n' || messageOnly.back() == '\r'))
        {
            messageOnly.remove_suffix(1);
        }
        
        // log Vulkan validation details
        auto& logger = Logger::getInstance();
        logger.enqueueLog(logLevel, __FILE__, __LINE__, "────────────── ⚠️ Vulkan Validation Message ──────────────");
        logger.enqueueLog(logLevel, __FILE__, __LINE__, "🔖 VUID: %s", messageIdName);
        logger.enqueueLog(logLevel, __FILE__, __LINE__, "🔎 Message ID: %d", messageId);
        logger.enqueueLog(logLevel, __FILE__, __LINE__, "📦 Type: %s", messageTypeStr.c_str());
        logger.enqueueLog(logLevel, __FILE__, __LINE__, "📝 Message: %.*s", static_cast<int>(messageOnly.size()), messageOnly.data());
        logger.enqueueLog(logLevel, __FILE__, __LINE__, "📜 Spec: %.*s", static_cast<int>(specOnly.size()), specOnly.data());

        // vulkan objects involved
        if (pCallbackData && pCallbackData->objectCount > 0 && pCallbackData->pObjects)
        {
            for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
            {
                const auto& object = pCallbackData->pObjects[i];
                const char* objectName = object.pObjectName ? object.pObjectName : "unknown";
                logger.enqueueLog(logLevel, __FILE__, __LINE__, "🔹 Object [%u] | name: %s | type: %s | handle: 0x%016llX",
                    i, objectName, string_VkObjectType(object.objectType), static_cast<unsigned long long>(object.objectHandle));
            }
        }

        logger.enqueueLog(logLevel, __FILE__, __LINE__, "────────────────────────────────────────────────────────");
        
        // The return value of this callback controls whether the vulkan call 
        // that caused the validation message will be aborted or not
        return VK_FALSE;
    }
}   // namespace keplar
