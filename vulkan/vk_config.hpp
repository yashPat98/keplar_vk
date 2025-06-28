// ────────────────────────────────────────────
//  File: vk_config.hpp · Created by Yash Patel · 6-27-2025
// ────────────────────────────────────────────

#pragma once

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

// enable validation layers in debug builds
#ifndef NDEBUG
    constexpr bool kEnableValidationLayers = true;
#else 
    constexpr bool kEnableValidationLayers = false;
#endif
