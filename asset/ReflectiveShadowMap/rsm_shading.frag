#version 460 core
#define PI 3.14159265
layout(location = 0) in vec2 iUV;
layout(location = 0) out vec4 oFragColor;

layout(binding = 0) uniform sampler2D GBuffer0;
layout(binding = 1) uniform sampler2D GBuffer1;
layout(binding = 2) uniform sampler2D LowResGBuffer0;
layout(binding = 3) uniform sampler2D LowResGBuffer1;
layout(binding = 4) uniform sampler2D LowResIndirect;


layout(std140, binding = 0) uniform Indirect{
    mat4 RSMLightViewProj;
    int PoissonDiskSampleCount;
    float IndirectSampleRadius;
    float LightProjWorldArea;
    int EnableIndirect;
};

layout(std140, binding = 1) uniform Light{
    vec3 LightDirection;
    vec3 LightRadiance;
};

layout(binding = 5) uniform sampler2D RSMBuffer0;
layout(binding = 6) uniform sampler2D RSMBuffer1;
layout(binding = 7) uniform sampler2D RSMBuffer2;

layout(std430, binding = 0) buffer PossionDiskSampleBuffer{
    vec4 PossionDiskSamples[];
};

vec4 worldToRSMClip(vec3 world_pos){
    return RSMLightViewProj * vec4(world_pos, 1.0);
}

vec3 decodeNormal(vec2 f)
{
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0, 1);
    n.xy += all(greaterThanEqual(n.xy, vec2(0.0))) ? vec2(-t) : vec2(t);
    return normalize(n);
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

        if(all(equal(clamp(rsm_pos, vec3(0), vec3(1)), rsm_pos))){
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

bool isLowResPixelGood(int x, int y, vec3 ideal_normal, vec3 ideal_color, out vec3 indirect){
    ivec2 res = textureSize(LowResIndirect, 0);
    vec2 uv = vec2(x + 0.5, y + 0.5) / res;

    vec4 lowres_indirect = texture(LowResIndirect, uv);
    if(lowres_indirect.w == 0)
        return false;

    vec4 gbuffer0 = texture(LowResGBuffer0, uv);
    vec4 gbuffer1 = texture(LowResGBuffer1, uv);
    vec3 normal = decodeNormal(vec2(gbuffer0.w, gbuffer1.w));
    vec3 color = gbuffer1.rgb;

    if(dot(normal, ideal_normal) < 0.8)
        return false;

    if(any(greater(color - ideal_color,vec3(0.1))))
        return false;

    indirect = lowres_indirect.rgb;

    return true;
}

vec3 getIndirect(vec2 uv, vec3 color, vec3 world_pos, vec3 world_normal){
    if(!EnableIndirect)
        return vec3(0);
    // disable low res indirect
    if(((EnableIndirect >> 1) & 1) == 0)
        return estimetaIndirect(worldToRMSClip(world_pos), world_pos, world_normal);

    ivec2 res = textureSize(LowResIndirect, 0);

    // [x, x + 0.5) -> x-1 and x
    // [x + 0.5, x + 1) -> x and x + 1
    int bx = int(ceil(uv.x * res.x - 0.5));
    int by = int(ceil(uv.y * res.y - 0.5));
    int ax = bx - 1;
    int ay = by - 1;

    // weight
    float tbx = uv.x * width - (ax + 0.5);
    float tby = uv.y * height - (ay + 0.5);
    float tax = 1 - tbx;
    float tay = 1 - tby;

    vec3 sum = vec3(0);
    vec3 indirect;
    float sum_weight = 0;
    int sum_count = 0;

    if(isLowResPixelGood(ax, ay, world_normal, color, indirect)){
        float weight = tax * tay;
        sum_weight += weight;// area weight
        sum += indirect * weight;
        sum_count += 1;
    }
    if(isLowResPixelGood(ax, by, world_normal, color, indirect)){
        float weight = tax * tby;
        sum_weight += weight;
        sum += indirect * weight;
        sum_count += 1;
    }
    if(isLowResPixelGood(bx, ay, world_normal, color, indirect)){
        float weight = tbx * tay;
        sum_weight += weight;
        sum += indirect * weight;
        sum_count += 1;
    }
    if(isLowResPixelGood(bx, by, world_normal, color, indirect)){
        float weight = tbx * tby;
        sum_weight += weight;
        sum += indirect * weight;
        sum_count += 1;
    }

    if(sum_count > 2)
        return sum / sum_weight;

    return estimetaIndirect(worldToRMSClip(world_pos), world_pos, world_normal);
}


void main(){
    vec4 gbuffer0 = texture(GBuffer0, iUV);
    if(gbuffer0.w > 50)
    discard;
    vec4 gbuffer1 = texture(GBuffer1, iUV);

    vec3 world_pos = gbuffer0.xyz;
    vec3 world_normal = decodeNormal(vec2(gbuffer0.w, gbuffer1.w));
    vec3 color = gbuffer1.rgb;
    // diffuse
    float cos_factor = max(0.0, dot(-world_normal, LightDirection));

    // indirect
    vec3 indirect = getIndirect(iUV, color, world_pos, world_normal);

    vec3 radiance = LightRadiance * color * (cos_factor + indirect);

    oFragColor = vec4(pow(radiance, vec3(1.0 / 2.2)), 1.0);
}

