// ────────────────────────────────────────────
//  File: config.hpp · Created by Yash Patel · 8-19-2025
// ────────────────────────────────────────────

#pragma once
#include <filesystem>

namespace keplar::config 
{
    inline constexpr const char* kWindowTitle              = "keplar_vk";
    inline constexpr int kDefaultWidth                     = 800;
    inline constexpr int kDefaultHeight                    = 600;

    static inline const std::filesystem::path kShaderDir   = "resources/shaders/";
    static inline const std::filesystem::path kTextureDir  = "resources/textures/";
    static inline const std::filesystem::path kModelDir    = "resources/models/";
} // namespace keplar::config
