#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in mat3 inTBN;

layout(location = 0) out vec4 outFragColor;

// ==========================================
// 绑定数据
// ==========================================
layout(set = 0, binding = 0) uniform SceneData {
    mat4 ViewProj;
    vec4 CameraPos;
    vec4 LightPos;
    vec4 LightColor;
} scene;

layout(set = 1, binding = 0) uniform sampler2D BaseColorMap;
layout(set = 1, binding = 1) uniform sampler2D NormalMap;

layout(set = 1, binding = 2) uniform MaterialParams {
    vec4  BaseColorTint;
    float Roughness;
    float Metallic;
    float NormalMapStrength;
    float Padding;
} params;

const float PI = 3.14159265359;

// =====================================================================
// 🛠️ Cook-Torrance BRDF 核心函数
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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// =====================================================================

void main() {
    // 1. 采样贴图和材质属性
    vec3 albedo = texture(BaseColorMap, inUV).rgb * params.BaseColorTint.rgb;
    float metallic = params.Metallic;
    float roughness = params.Roughness;
    float normalStrength = params.NormalMapStrength;

    // 2. 从法线贴图采样并应用到切线空间
    vec3 sampledNormal = texture(NormalMap, inUV).xyz;
    
    // 检查是否是白色贴图 (fallback)，如果是则直接使用几何法线
    vec3 N;
    if (length(sampledNormal - vec3(1.0)) < 0.01) {
        // 没有法线贴图，直接使用几何法线
        N = normalize(inNormal);
    } else {
        // 有法线贴图，使用 TBN 矩阵
        vec3 tangentNormal = sampledNormal * 2.0 - 1.0;
        tangentNormal.xy *= normalStrength;
        N = normalize(inTBN * tangentNormal);
    }

    vec3 V = normalize(scene.CameraPos.xyz - inWorldPos);
    vec3 L = normalize(scene.LightPos.xyz - inWorldPos);
    vec3 H = normalize(V + L);

    // 3. 计算基础反射率 F0
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // 4. 计算光线辐射度与平方反比衰减
    float distance = length(scene.LightPos.xyz - inWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = scene.LightColor.rgb * scene.LightColor.a * attenuation;

    // --- 开始组装 Cook-Torrance BRDF ---

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
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // --- 结束 Cook-Torrance BRDF ---

    // 5. 简单环境光
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // 6. HDR 色调映射
    color = color / (color + vec3(1.0));

    outFragColor = vec4(color, 1.0);
}
