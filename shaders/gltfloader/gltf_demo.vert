#version 450 core 
#extension GL_ARB_seperate_shader_objects : enable 

// -------------------------------------
// vertex inputs
// -------------------------------------

layout(location = 0) in vec4 inPosition; 
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV; 
layout(location = 3) in vec4 inTangent;
// layout(location = 4) in vec4 inBoneIndices;
// layout(location = 5) in vec4 inBoneWeights;

// ----------------------------
// output to fragment shader (varyings)
// ----------------------------

layout(location = 0) out vec2 vUV;
// layout(location = 1) out vec3 vNormal;
// layout(location = 2) out vec3 vTangent;
// layout(location = 3) out vec3 vBitangent;

// -------------------------------------
// descriptor set 0: camera / per-frame data
// -------------------------------------

layout(set = 0, binding = 0) uniform CameraUBO
{ 
    mat4 projection;
    mat4 view; 
    mat4 model;
} camera; 

// layout(set = 1, binding = 0) uniform SkinMatricesUBO
// {
//    mat4 boneMatrices[128];
// } skin;

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
// vertex stage entry point 
// -------------------------------------

void main(void) 
{ 
    vUV = inUV;
    gl_Position = camera.projection * camera.view * camera.model * pc.model * inPosition;
}
