// ────────────────────────────────────────────
//  File: vk_utils.hpp · Created by Yash Patel · 6-27-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

// khronos validation layer macros to avoid typos
#define VK_LAYER_KHRONOS_VALIDATION_NAME                            "VK_LAYER_KHRONOS_validation"
#define VK_LAYER_LUNARG_API_DUMP_NAME                               "VK_LAYER_LUNARG_api_dump"
#define VK_LAYER_LUNARG_MONITOR_NAME                                "VK_LAYER_LUNARG_monitor"
#define VK_LAYER_LUNARG_DEVICE_SIMULATION_NAME                      "VK_LAYER_LUNARG_device_simulation"
#define VK_LAYER_LUNARG_SCREENSHOT_NAME                             "VK_LAYER_LUNARG_screenshot"
#define VK_LAYER_LUNARG_GFXRECONSTRUCT_NAME                         "VK_LAYER_LUNARG_gfxreconstruct"
#define VK_LAYER_LUNARG_CRASH_DIAGNOSTIC_NAME                       "VK_LAYER_LUNARG_crash_diagnostic"
#define VK_LAYER_KHRONOS_PROFILES_NAME                              "VK_LAYER_KHRONOS_profiles"
#define VK_LAYER_KHRONOS_SYNCHRONIZATION2_NAME                      "VK_LAYER_KHRONOS_synchronization2"
#define VK_LAYER_RENDERDOC_CAPTURE_NAME                             "VK_LAYER_RENDERDOC_Capture"
#define VK_LAYER_KHRONOS_SHADER_OBJECT_NAME                         "VK_LAYER_KHRONOS_shader_object"
#define VK_LAYER_NV_OPTIMUS_NAME                                    "VK_LAYER_NV_optimus"
#define VK_LAYER_NV_NOMAD_RELEASE_PUBLIC_2025_1_1_NAME              "VK_LAYER_NV_nomad_release_public_2025_1_1"
#define VK_LAYER_NV_PRESENT_NAME                                    "VK_LAYER_NV_present"
#define VK_LAYER_NV_GPU_TRACE_RELEASE_PUBLIC_2025_1_1_NAME          "VK_LAYER_NV_GPU_Trace_release_public_2025_1_1"
#define VK_LAYER_NV_SHADER_DEBUGGER_RELEASE_PUBLIC_2025_1_1_NAME    "VK_LAYER_NV_shader_debugger_release_public_2025_1_1"
#define VK_LAYER_OBS_HOOK_NAME                                      "VK_LAYER_OBS_HOOK"

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
