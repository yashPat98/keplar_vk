// ────────────────────────────────────────────
//  File: common.hpp · Created by Yash Patel · 10-04-2025
// ────────────────────────────────────────────

#pragma once

// disable external header warnings
#pragma warning(push, 0)

// tinygltf + stb headers
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include <tiny_gltf.h>
#include <stb_image.h>

#pragma warning(pop)

