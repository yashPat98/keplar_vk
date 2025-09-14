// ────────────────────────────────────────────
//  File: vulkan_config.hpp · Created by Yash Patel · 6-27-2025
// ────────────────────────────────────────────

#pragma once

// std headers
#include <vector>
#include <string_view>

// define platform-specific Vulkan surface extension
#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
    #define VK_USE_PLATFORM_XLIB_KHR
#else
    #error "Unsupported Vulkan platform"
#endif

// include Vulkan headers
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

namespace keplar 
{
    // default configuration constants
    inline constexpr std::string_view kDefaultApplicationName    = "keplar_vk";
    inline constexpr uint32_t         kDefaultApplicationVersion = VK_MAKE_VERSION(1, 0, 0);
    inline constexpr std::string_view kDefaultEngineName         = "keplar_vk";
    inline constexpr uint32_t         kDefaultEngineVersion      = VK_MAKE_VERSION(1, 0, 0);
    inline constexpr uint32_t         kDefaultVulkanApiVersion   = VK_API_VERSION_1_4;

    // vulkan configuration structure
    struct VulkanContextConfig
    {
        // application info
        std::string_view mApplicationName    = kDefaultApplicationName;
        uint32_t         mApplicationVersion = kDefaultApplicationVersion;
        std::string_view mEngineName         = kDefaultEngineName;
        uint32_t         mEngineVersion      = kDefaultEngineVersion;
        uint32_t         mApiVersion         = kDefaultVulkanApiVersion;

        // enable validation layers in debug mode by default
        #ifndef NDEBUG
            bool mEnableValidation = true;
        #else
            bool mEnableValidation = false;
        #endif

        // extensions and validation layers
        std::vector<std::string_view> mInstanceExtensions;
        std::vector<std::string_view> mValidationLayers;
        std::vector<std::string_view> mDeviceExtensions;

        // requested physical device features
        VkPhysicalDeviceFeatures mRequestedFeatures;

        // hint to prefer dedicated queue families 
        bool mPreferDedicatedComputeQueue = false;
        bool mPreferDedicatedTransferQueue = false;
    };
}  // namespace keplar
