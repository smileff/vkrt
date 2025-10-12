#version 450

// Vertex shader input
layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Color2;

layout(location = 0) out vec3 FragColor;
layout(location = 1) out vec3 FragColor2;

layout(push_constant) uniform PushConstants {
    vec4 WorldViewMatRow0;
    vec4 WorldViewMatRow1;
    vec4 WorldViewMatRow2;
    vec4 ProjectionFactors;
};

void main() 
{
    vec4 modelPos = vec4(Position, 1.0);

    float xOffset = -0.5 + float(gl_InstanceIndex) * 0.25;    
    modelPos[0] += xOffset;

    vec4 viewPos = vec4(
        dot(WorldViewMatRow0, modelPos),
        dot(WorldViewMatRow1, modelPos),
        dot(WorldViewMatRow2, modelPos),
        1.0);
    vec4 clipPos = vec4(
        viewPos[0] * ProjectionFactors[0],
        viewPos[1] * ProjectionFactors[1],
        viewPos[2] * ProjectionFactors[2] + ProjectionFactors[3],
        viewPos[2]);

    gl_Position = clipPos / clipPos.w;
    FragColor = Color;
    FragColor2 = Color2;
}