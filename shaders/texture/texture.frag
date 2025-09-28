#version 450 core 
#extension GL_ARB_separate_shader_object : enable

// input from previous stage
layout(location = 0) in vec2 inTexcoord;

// output to color attachment
layout(location = 0) out vec4 fragColor;

// uniforms
layout(binding = 1) uniform sampler2D textureSampler;

// fragment shader entry point
void main(void)
{
    fragColor = texture(textureSampler, inTexcoord);
}
