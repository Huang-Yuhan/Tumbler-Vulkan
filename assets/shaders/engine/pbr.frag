#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outFragColor;

// 【新增】Binding 0: 全局场景参数
layout(set = 0, binding = 0) uniform SceneData {
    mat4 ViewProj;
    vec4 CameraPos;
    vec4 LightPos;
    vec4 LightColor;
} scene;

// 【修改】材质数据退居 Set 1
layout(set = 1, binding = 0) uniform sampler2D BaseColorMap;

layout(set = 1, binding = 1) uniform MaterialParams {
    vec4  BaseColorTint;
    float Roughness;
    float Metallic;
    vec2  Padding;
} params;

void main() {
    // 基础颜色 (反照率)
    vec4 albedo = texture(BaseColorMap, inUV) * params.BaseColorTint;

    // 向量准备
    vec3 N = normalize(inNormal);
    vec3 L = normalize(scene.LightPos.xyz - inWorldPos);

    // 【简易光照测试】最基础的 Lambert 漫反射 (N dot L)
    // 如果法线和光线方向一致，最亮 (1.0)；如果背对光线，则全黑 (0.0)
    float NdotL = max(dot(N, L), 0.0);

    // 添加一点极其微弱的环境光，防止暗部彻底死黑
    vec3 ambient = albedo.rgb * 0.05;

    // 最终颜色 = 环境光 + (基础色 * 光照强度)
    vec3 finalColor = ambient + (albedo.rgb * NdotL);

    outFragColor = vec4(finalColor, 1.0);
}