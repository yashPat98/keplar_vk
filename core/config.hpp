// ────────────────────────────────────────────
//  File: config.hpp · Created by Yash Patel · 8-19-2025
// ────────────────────────────────────────────

#pragma once
#include <filesystem>

namespace keplar::config 
{
    static inline const std::filesystem::path kShaderDir   = "resources/shaders/";
    static inline const std::filesystem::path kTextureDir  = "resources/textures/";
    static inline const std::filesystem::path kModelDir    = "resources/models/";
} // namespace keplar::config
