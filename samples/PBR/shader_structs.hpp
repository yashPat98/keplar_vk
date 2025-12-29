// ────────────────────────────────────────────
//  File: shader_structs.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "graphics/math3d.hpp"

namespace keplar::ubo
{
    struct alignas(16) Camera
    {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 position;
    };

    constexpr int MAX_LIGHTS = 1;
    struct alignas(16) Light
    {
        glm::vec4 position[MAX_LIGHTS];
        glm::vec4 color[MAX_LIGHTS];
        glm::ivec4 count;
    };
}   // namespace keplar
