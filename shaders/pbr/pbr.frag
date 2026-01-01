#version 450 core
#extension GL_ARB_seperate_shader_objects : enable

// -------------------------------------
// inputs from vertex shader
// -------------------------------------

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;

// -------------------------------------
// fragment output
// -------------------------------------

layout(location = 0) out vec4 fragColor;

// -------------------------------------
// descriptor set 0: camera / per-frame data
// -------------------------------------

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 position;
} camera;

// -------------------------------------
// descriptor set 1: pbr textures
// -------------------------------------

layout(set = 1, binding = 1) uniform sampler2D uBaseColorMap;
layout(set = 1, binding = 2) uniform sampler2D uMetallicRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D uNormalMap;
layout(set = 1, binding = 4) uniform sampler2D uOcclusionMap;
layout(set = 1, binding = 5) uniform sampler2D uEmissiveMap;

// -------------------------------------
// descriptor set 2: lighting data
// -------------------------------------

layout(constant_id = 0) const int MAX_LIGHTS = 1;
layout(set = 2, binding = 0) uniform LightUBO
{
    vec4 position[MAX_LIGHTS];
    vec4 color[MAX_LIGHTS];
    ivec4 count;
} lights;

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
// PBR Microfacet Model Functions
// -------------------------------------

#define PI 3.14159265359

vec3 freshnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom + 0.0001f);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 calculateNormal()
{
    vec3 tangentNormal = texture(uNormalMap, vUV).xyz * 2.0f - 1.0f;
    vec3 T = normalize(vTangent);
    vec3 B = normalize(vBitangent);
    vec3 N = normalize(vNormal);
    return normalize(mat3(T, B, N) * tangentNormal);
}

// -------------------------------------
// fragment stage entry point
// -------------------------------------

void main(void)
{   
    // base color (sRGB -> linear)
    vec3 albedo = pow(texture(uBaseColorMap, vUV).rgb, vec3(2.2f)) * pc.baseColor.rgb;
    
    // metallic roughness (gltf standard)
    vec4 mr = texture(uMetallicRoughnessMap, vUV);
    float metallic = mr.b * pc.pbrFactors.x;
    float roughness = mr.g * pc.pbrFactors.y;
    
    // sample ambient occlusion and emissive color
    float ao = texture(uOcclusionMap, vUV).r;
    vec3 emissive = texture(uEmissiveMap, vUV).rgb * pc.emissiveColor.rgb;

    // compute normal and view direction
    vec3 N = calculateNormal();
    vec3 V = normalize(camera.position.xyz - vWorldPos);

    // base reflectance
    vec3 F0 = mix(vec3(0.04f), albedo, metallic);
    vec3 Lo = vec3(0.0f);

    // accumulate lighting from all direct light sources
    for (int i = 0; i < lights.count.x; i++)
    {
        vec3 L = normalize(lights.position[i].xyz - vWorldPos);
        vec3 H = normalize(V + L);

        float dist = length(lights.position[i].xyz - vWorldPos);
        vec3 radiance = (lights.color[i].rgb * lights.color[i].w) / (dist * dist);

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = freshnelSchlick(max(dot(H, V), 0.0f), F0);

        vec3 specular = (NDF * G * F) / (4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);
        vec3 kS = F;
        vec3 kD = (1.0f - kS) * (1.0f - metallic);
        
        float NdotL = max(dot(N, L), 0.0f);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // combine ambient, direct lighting, and emissive contributions
    vec3 ambient = vec3(0.05f) * albedo * ao;
    vec3 color = ambient + Lo + emissive;

    // apply tonemap
    color = color / (color + vec3(1.0f));

    // apply gamma 
    color = pow(color, vec3(1.0f / 2.2f));

    // final color
    fragColor = vec4(color, 1.0f);
}
