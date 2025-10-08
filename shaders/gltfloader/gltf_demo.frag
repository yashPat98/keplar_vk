#version 450 core 
#extension GL_ARB_separate_shader_object : enable

// -------------------------------------
// inputs from vertex shader
// -------------------------------------

layout(location = 0) in vec2 vUV; 
// layout(location = 1) in vec3 vNormal;
// layout(location = 2) in vec3 vTangent;
// layout(location = 3) in vec3 vBitangent;

// -------------------------------------
// fragment output
// -------------------------------------

layout(location = 0) out vec4 fragColor;

// -------------------------------------
// descriptor set 1: pbr textures
// -------------------------------------

layout(set = 1, binding = 1) uniform sampler2D uBaseColorMap;
layout(set = 1, binding = 2) uniform sampler2D uMetallicRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D uNormalMap;
layout(set = 1, binding = 4) uniform sampler2D uOcclusionMap;
layout(set = 1, binding = 5) uniform sampler2D uEmissiveMap;

// -------------------------------------
// push constants: shared vertex + fragment stage
// -------------------------------------

layout(push_constant) uniform PushConstants 
{
    mat4 model;          // 64 bytes: model matrix
    vec4 baseColor;      // 16 bytes: r,g,b,a
    vec4 pbrFactors;     // 16 bytes: x:metallic, y:roughness, z:specular, w:unused
    vec4 emissiveColor;  // 16 bytes: r,g,b + padding
} pc;

// -------------------------------------
// fragment stage entry point
// -------------------------------------

void main(void)
{
    fragColor = texture(uBaseColorMap, vUV);
}
