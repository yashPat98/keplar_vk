#version 450 core 
#extension GL_ARB_seperate_shader_objects : enable 

// vertex input 
layout(location = 0) in vec4 inPosition; 
layout(location = 1) in vec4 inColor; 

// vertex output 
layout(location = 0) out vec4 outColor; 

// uniform buffer 
layout(std140, binding = 0) uniform UBO_FrameData
{ 
    mat4 model; 
    mat4 view; 
    mat4 projection; 
} uboFrameData; 

// vertex shader entry point 
void main(void) 
{ 
    outColor = inColor; 
    gl_Position = uboFrameData.projection * uboFrameData.view * uboFrameData.model * inPosition; 
}
