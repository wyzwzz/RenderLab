#version 460 core
layout(location = 0) in vec3 VertexPos;
layout(location = 1) in vec3 VertexNormal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec3 VertexTangent;
//layout(location = 4) in vec3 VertexBitangent;

layout(location = 0) out VS_OUT{
    vec3 FragPos;
    vec2 TexCoord;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
}vs_out;

layout(std140, binding = 0) uniform Transform{
    mat4 Model;
    mat4 MVP;
};
layout(std140, binding = 1) uniform PerFrame{
    vec3 ViewPos;
};
layout(std140, binding = 2) uniform Params{
    vec3 LightPos;
    float HeightScale;
    vec3 LightRadiance;
    int MapMethod;
};

void main(){
    gl_Position = MVP * vec4(VertexPos, 1.0);

    vs_out.FragPos = vec3(Model * vec4(VertexPos, 1.0));
    vs_out.TexCoord = TexCoord;

    vec3 T = normalize(mat3(Model) * VertexTangent);
    vec3 B = normalize(mat3(Model) * normalize(-cross(VertexNormal, VertexTangent)));
    vec3 N = normalize(mat3(Model) * VertexNormal);
    mat3 TBN = transpose(mat3(T, B, N));
    vs_out.TangentLightPos = TBN * LightPos;
    vs_out.TangentViewPos = TBN * ViewPos;
    vs_out.TangentFragPos = TBN * vs_out.FragPos;
}

