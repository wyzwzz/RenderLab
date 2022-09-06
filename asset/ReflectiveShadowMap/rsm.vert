#version 460 core

layout(location = 0) in vec3 VertexPos;
layout(location = 1) in vec3 VertexNormal;
layout(location = 2) in vec3 VertexAlbedo;

layout(location = 0) out vec3 WorldPos;
layout(location = 1) out vec3 WorldNormal;
layout(location = 2) out vec3 WorldAlbedo;

layout(std140, binding = 0) uniform Transform{
    mat4 Model;
    mat4 MVP;
};

void main(){
    gl_Position = MVP * vec4(VertexPos, 1.0);
    WorldPos = vec3(Model * vec4(WorldPos, 1.0));
    WorldNormal = vec3(Model * vec4(WorldNormal, 0.0));
    WorldAlbedo = VertexAlbedo;
}