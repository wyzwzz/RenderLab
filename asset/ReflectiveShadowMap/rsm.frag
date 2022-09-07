#version 460 core

layout(location = 0) in vec3 iFragPos;
layout(location = 1) in vec3 iFragNormal;
layout(location = 2) in vec2 iFragUV;

layout(location = 0) out vec4 oFragPos;
layout(location = 1) out vec4 oFragNormal;
layout(location = 2) out vec4 oRadiance;

layout(binding = 0) uniform sampler2D AlbedoMap;

layout(std140, binding = 1) uniform Light{
    vec3 LightDirection;
    vec3 LightRadiance;
};

void main(){
    oFragPos = vec4(iFragPos, 1.0);
    oFragNormal = vec4(iFragNormal, 1.0);
    vec3 albedo = texture(AlbedoMap, iFragUV).rgb;
    oRadiance = vec4(LightRadiance * albedo, 1.0);
}