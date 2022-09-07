#version 460 core
#define PI 3.14159265
layout(location = 0) in vec2 iUV;
layout(location = 0) out vec4 LowResGBuffer0;
layout(location = 1) out vec4 LowResGBuffer1;
layout(location = 2) out vec4 oFragColor;

layout(binding = 0) uniform sampler2D GBuffer0;
layout(binding = 1) uniform sampler2D GBuffer1;

layout(binding = 5) uniform sampler2D RSMBuffer0;
layout(binding = 6) uniform sampler2D RSMBuffer1;
layout(binding = 7) uniform sampler2D RSMBuffer2;

layout(std140, binding = 0) uniform Indirect{
    mat4 RSMLightViewProj;
    int PoissonDiskSampleCount;
    float IndirectSampleRadius;
    float LightProjWorldArea;
    int EnableIndirect;
};

layout(std430, binding = 0) buffer PossionDiskSampleBuffer{
    vec4 PossionDiskSamples[];
};

vec4 worldToRSMClip(vec3 world_pos){
    return RSMLightViewProj * vec4(world_pos, 1.0);
}

// rsmClipPos is world pos transformed by LightVP
// seach pos nearby rsmClipPos and sample in RSMBuffer while add with weight
vec3 estimetaIndirect(vec4 rsmClipPos, vec3 pos, vec3 normal){
    // clip pos -> [0, 1]
    vec2 rsm_center = 0.5 * rsmClipPos.xy / rsmClipPos.w + 0.5;
    vec3 sum = vec3(0);
    for(int i = 0; i < PoissonDiskSampleCount; i++){
        vec3 raw_sample = PossionDiskSamples[i].xyz;
        vec2 rsm_pos = rsm_center + raw_sample.xy;
        float weight = raw_sample.z;

        if(all(equal(clamp(rsm_pos, vec2(0), vec2(1)), rsm_pos))){
            vec3 world_pos = texture(RSMBuffer0, rsm_pos).xyz;
            vec3 world_normal = texture(RSMBuffer1, rsm_pos).xyz;
            vec3 flux = texture(RSMBuffer2, rsm_pos).xyz;

            vec3 diff = world_pos - pos;
            float dist2 = dot(diff, diff);

            // point light
            sum += weight * flux
            / max(dist2 * dist2, 0.001) // attenuation
            * max(0.0, dot(normal, diff)) // cos factor for reveice
            * max(0.0, dot(world_normal, -diff)); // cos factor for emit
        }
    }
    float area_ratio = PI * IndirectSampleRadius * IndirectSampleRadius;
    return area_ratio * LightProjWorldArea * sum;
}

vec3 decodeNormal(vec2 f)
{
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0, 1);
    n.xy += vec2((n.x >= 0 ? -t : t), (n.y >= 0 ? -t : t));
    return normalize(n);
}

void main(){
    vec4 gbuffer0 = texture(GBuffer0, iUV);
    if(gbuffer0.w > 50)
        discard;
    vec4 gbuffer1 = texture(GBuffer1, iUV);

    vec3 world_pos = gbuffer0.xyz;
    vec3 world_normal = decodeNormal(vec2(gbuffer0.w, gbuffer1.w));

    vec4 rsm_clip_pos = worldToRSMClip(world_pos);
    vec3 indirect = estimetaIndirect(rsm_clip_pos, world_pos, world_normal);

    LowResGBuffer0 = vec4(world_normal, gbuffer0.w);
    LowResGBuffer1 = vec4(gbuffer1.rgb, gbuffer1.w);

    oFragColor = vec4(indirect, 1.0);
}