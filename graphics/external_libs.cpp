// ────────────────────────────────────────────
//  File: common.cpp · Created by Yash Patel · 10-04-2025
// ────────────────────────────────────────────

// disable noisy warnings from vendor code
#pragma warning(push, 0)

// tinygltf implementation
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION

#include <tiny_gltf.h>

#pragma warning(pop)
