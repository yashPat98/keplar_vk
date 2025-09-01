#version 450 core 
#extension GL_ARB_separate_shader_object : enable

// input from previous stage
layout(location = 0) in vec4 inColor;

// output to color attachment
layout(location = 0) out vec4 fragColor;

// fragment shader entry point
void main(void)
{
    fragColor = inColor;
}
