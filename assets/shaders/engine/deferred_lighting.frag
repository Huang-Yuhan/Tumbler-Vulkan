#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

// ==========================================
// 全局场景数据 (由 FDeferredPipeline 主动传入)
// ==========================================
struct PointLightData {
    vec4 Position;
    vec4 Color;
};

layout(set = 0, binding = 0) uniform SceneData {
    mat4 ViewProj;
    mat4 InvViewProj;
    vec4 CameraPos;
    PointLightData Lights[8];
    int LightCount;
} scene;

// ==========================================
// G-Buffer 局部内存采样 (Input Attachments)
// 这三个分别是 pInputAttachments 数组的 [0], [1], [2] 附件。
// ==========================================
layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inAlbedoMetallic;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inNormalRoughness;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput inDepth;

const float PI = 3.14159265359;

// =====================================================================
// PBR 核心计算 (复用原有逻辑)
// =====================================================================
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 1. 从片内共享缓存极速拉取 G-Buffer 数据
    float depth = subpassLoad(inDepth).r;
    vec4 albedoMetal = subpassLoad(inAlbedoMetallic);
    vec4 normalRough = subpassLoad(inNormalRoughness);

    // 如果深度为 1.0 (远裁剪面 / 天空盒)，跳过打光并直接刷底色
    if (depth == 1.0) {
        // 如果我们有独立环境或者天空盒逻辑应该在这里触发
        outFragColor = vec4(albedoMetal.rgb, 1.0);
        return;
    }

    vec3 albedo = albedoMetal.rgb;
    float metallic = albedoMetal.a;
    vec3 N = normalize(normalRough.xyz);
    float roughness = normalRough.a;

    // 2. 世界坐标还原重建 (Depth Unprojection)
    // 根据 UV (0~1) 映射回 Normalized Device Coordinates (-1~1)
    vec4 clipSpace = vec4(inUV * 2.0 - 1.0, depth, 1.0);
    vec4 worldPosW = scene.InvViewProj * clipSpace;
    vec3 worldPos = worldPosW.xyz / worldPosW.w;

    vec3 V = normalize(scene.CameraPos.xyz - worldPos);

    // 3. 计算基础反射率 F0
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < scene.LightCount; i++) {
        vec3 L = normalize(scene.Lights[i].Position.xyz - worldPos);
        vec3 H = normalize(V + L);

        float dist = length(scene.Lights[i].Position.xyz - worldPos);
        float attenuation = 1.0 / (dist * dist);
        vec3 radiance = scene.Lights[i].Color.rgb * scene.Lights[i].Color.a * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 5. 环境光遮蔽与自发光兜底
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // 6. HDR 色调映射
    color = color / (color + vec3(1.0));

    outFragColor = vec4(color, 1.0);
}
