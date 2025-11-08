// ────────────────────────────────────────────
//  File: keplar_config.hpp · Created by Yash Patel · 8-19-2025
// ────────────────────────────────────────────

#pragma once
#include <filesystem>

namespace keplar::config 
{
    inline constexpr const char* kWindowTitle              = "keplar";
    inline constexpr int kDefaultWidth                     = 1920;
    inline constexpr int kDefaultHeight                    = 1080;
    inline constexpr bool kStartMaximized                  = true;
    inline constexpr float kDefaultFrameRate               = 120.0f;

    static inline const std::filesystem::path kShaderDir   = "resources/shaders/";
    static inline const std::filesystem::path kTextureDir  = "resources/textures/";
    static inline const std::filesystem::path kModelDir    = "resources/models/";
} // namespace keplar::config
