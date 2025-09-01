// ────────────────────────────────────────────
//  File: shader_structs.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vk_config.hpp"

namespace keplar::ubo
{
    struct alignas(16) FrameData
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };
}   // namespace keplar
