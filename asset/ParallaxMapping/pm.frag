#version 460 core
#define PM_DEPTHCORRECT
#define PM_SHADOWS 0
#define NM 0
#define POM 1
#define RPM 2
layout(location = 0) in VS_OUT{
    vec3 FragPos;
    vec2 TexCoord;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
}fs_in;

layout(location = 0) out vec4 oFragColor;

layout(binding = 0) uniform sampler2D DepthMap;
layout(binding = 1) uniform sampler2D AlbedoMap;
layout(binding = 2) uniform sampler2D NormalMap;


layout(std140, binding = 1) uniform PerFrame{
    vec3 ViewPos;
};
layout(std140, binding = 2) uniform Params{
    vec3 LightPos;
    float HeightScale;
    vec3 LightRadiance;
    int MapMethod;
};

// parallax occlusion mapping
// view_dir : tangent space view direction form frag pos to view pos
vec2 pom_trace(in vec2 start_uv, in vec3 view_dir){
    const float min_linear_seach_steps = 8;
    const float max_linear_seach_steps = 32;
    // steps get large if angle between local normal and view_dir get large
    float linear_seach_steps = mix(max_linear_seach_steps, min_linear_seach_steps, abs(dot(vec3(0.0, 0.0, 1.0), view_dir)));
    float step_depth = 1.0 / linear_seach_steps;
    float cur_depth = 0.0;
    // max texcoord shift with depth from 0 to 1
    // HeightScale is the same with the inverse of view_dir.z and not affect the light shading but affect albdo
    // view_dir will both affect albedo and light shading
    vec2 depth_delta_uv = view_dir.xy / view_dir.z * HeightScale;
    depth_delta_uv /= linear_seach_steps;

    vec2 cur_uv = start_uv;
    float pos_depth = texture(DepthMap, cur_uv).r;
    while(cur_depth < pos_depth){
        cur_uv -= depth_delta_uv;
        pos_depth = texture(DepthMap, cur_uv).r;
        cur_depth += step_depth;
    }

    vec2 prev_uv = cur_uv + depth_delta_uv;
    float after_depth = cur_depth - pos_depth;
    float before_depth = texture(DepthMap, prev_uv).r - (cur_depth - step_depth);

    float weight = after_depth / (after_depth + before_depth);
    return mix(cur_uv, prev_uv, weight);
}
vec2 rpm_trace(in vec2 start_uv, in vec3 view_dir){
    const float min_linear_seach_steps = 8;
    const float max_linear_seach_steps = 32;
    // steps get large if angle between local normal and view_dir get large
    float linear_seach_steps = mix(max_linear_seach_steps, min_linear_seach_steps, abs(dot(vec3(0.0, 0.0, 1.0), view_dir)));
    float step_depth = 1.0 / linear_seach_steps;
    float cur_depth = 0.0;
    // max texcoord shift with depth from 0 to 1
    // HeightScale is the same with the inverse of view_dir.z and not affect the light shading but affect albdo
    // view_dir will both affect albedo and light shading
    vec2 depth_delta_uv = view_dir.xy / view_dir.z * HeightScale;
    depth_delta_uv /= linear_seach_steps;

    vec2 cur_uv = start_uv;
    float pos_depth = texture(DepthMap, cur_uv).r;
    while(cur_depth < pos_depth){
        cur_uv -= depth_delta_uv;
        pos_depth = texture(DepthMap, cur_uv).r;
        cur_depth += step_depth;
    }
    // binary search
    vec2 prev_uv = cur_uv + depth_delta_uv;
    float prev_depth = cur_depth - step_depth;
    vec2 next_uv = cur_uv;
    float next_depth = cur_depth;
    const int binary_search_steps = 8;
    for(int i = 0; i < binary_search_steps; i++){
        vec2 mid_uv = (prev_uv + next_uv) * 0.5;
        float mid_depth = (prev_depth + next_depth) * 0.5;
        float mid_pos_depth = texture(DepthMap, mid_uv).r;
        if(prev_uv == next_uv) break;
        if(mid_depth <= mid_pos_depth){
            prev_uv = mid_uv;
            prev_depth = mid_depth;
        }
        else{
            next_uv = mid_uv;
            next_depth = mid_depth;
        }
    }
    return next_uv;
}
void main(){
    vec3 tangent_view_dir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec2 frag_uv = fs_in.TexCoord;

    vec2 view_frag_uv;
    if(MapMethod == POM){
        view_frag_uv = pom_trace(frag_uv, tangent_view_dir);
    }
    else if(MapMethod == RPM){
        view_frag_uv = rpm_trace(frag_uv, tangent_view_dir);
    }
    else{
        view_frag_uv = frag_uv;
    }

    vec3 tangent_normal = texture(NormalMap, view_frag_uv).xyz;
    tangent_normal = normalize(tangent_normal * 2.0 - 1.0);
    vec3 albedo = texture(AlbedoMap, view_frag_uv).rgb;
    albedo = pow(albedo, vec3(2.2));

    vec3 ambient = vec3(0.1) * albedo;
    vec3 tangent_light_dir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    vec3 diffuse = max(dot(tangent_light_dir, tangent_normal), 0.0) * albedo;
    vec3 specular = pow(max(0, dot(normalize(tangent_light_dir + tangent_view_dir), tangent_normal)), 32.0) * vec3(0.2);
#if PM_SHADOWS
    if(MapMethod != NM){
        vec2 light_frag_uv = view_frag_uv + tangent_light_dir.xy / tangent_light_dir.z * HeightScale;
        vec2 uv;
        if(MapMethod == POM){
            uv = pom_trace(light_frag_uv, tangent_light_dir);
        }
        else if(MapMethod == RPM){
            uv = rpm_trace(light_frag_uv, tangent_light_dir);
        }
        if(length(uv - light_frag_uv) < length(view_frag_uv - light_frag_uv) - 0.05){
            specular = vec3(0);
            diffuse = vec3(0.333) * ambient;
        }
    }
#endif
    vec3 color = (ambient + diffuse + specular) * LightRadiance;
    color = pow(color, vec3(1.0/2.2));
    oFragColor = vec4(color, 1.0);
}
