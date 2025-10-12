#version 450

layout(location = 0) in vec3 FragColor;
layout(location = 1) in vec3 FragColor2;

layout(location = 0) out vec4 OutColor;

layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 UniformColor2;
};

void main() 
{
    if (gl_FrontFacing) {
        OutColor = vec4(FragColor, 1.0f);
    } else {
        OutColor = vec4(FragColor2, 1.0);
    }
}