// ────────────────────────────────────────────
//  File: vk_utils.hpp · Created by Yash Patel · 6-27-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

namespace keplar
{
    // macro to checks vulkan call, logs error if not VK_SUCCESS, returns true/false.
    #define VK_CHECK(call) ([&]() -> bool \
    { \
        VkResult _vk_result = (call); \
        if (_vk_result != VK_SUCCESS) \
        { \
            keplar::Logger::getInstance().enqueueLog( \
                keplar::Logger::Level::Fatal, __FILE__, __LINE__, \
                "%s failed with error %s [code: %d]", #call, string_VkResult(_vk_result), _vk_result); \
            return false; \
        } \
        return true; \
    })()
}   // namespace keplar
