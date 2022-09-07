#version 460 core
layout(location = 0) in vec3 iFragPos;
layout(location = 1) in vec3 iFragNormal;
layout(location = 2) in vec2 iFragUV;

layout(location = 0) out vec4 GBuffer0;
layout(location = 1) out vec4 GBuffer1;

layout(binding = 0) uniform sampler2D AlbedoMap;

vec2 octWrap(vec2 v)
{
    return (1.0 - abs(v.yx)) * vec2((v.x >= 0.0 ? 1.0 : -1.0), (v.y >= 0.0 ? 1.0 : -1.0));
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
// https://www.shadertoy.com/view/llfcRl
vec2 encodeNormal(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : octWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

void main(){
    vec2 oct_normal = encodeNormal(normalize(iFragNormal));
    vec3 albedo = texture(AlbedoMap, iFragUV).rgb;
    GBuffer0 = vec4(iFragPos, oct_normal.x);
    GBuffer1 = vec4(albedo, oct_normal.y);
}