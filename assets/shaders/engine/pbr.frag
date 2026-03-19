#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;

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

layout(set = 1, binding = 1) uniform MaterialParams {
    vec4  BaseColorTint;
    float Roughness;
    float Metallic;
    vec2  Padding;
} params;

const float PI = 3.14159265359;

// =====================================================================
// 🛠️ 你的试炼场：填空这三个 Cook-Torrance BRDF 核心函数！
// =====================================================================

// D - 法线分布函数 (Normal Distribution Function)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001); // 加上极小值防止除以0崩溃
}

// 几何遮蔽的子函数：计算单侧的遮蔽情况
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    // 注意：k 的这种算算法仅适用于“直接光照”！如果以后做 IBL(环境光)，k的公式会变。
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// G - 几何遮蔽函数主函数 (Geometry Function)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggx2 = GeometrySchlickGGX(NdotV, roughness); // 视线方向的几何遮挡
    float ggx1 = GeometrySchlickGGX(NdotL, roughness); // 光线方向的几何遮挡

    return ggx1 * ggx2;
}
/// F - 菲涅尔方程 (Fresnel Equation)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    // cosTheta 通常传入的是 max(dot(H, V), 0.0)
    // F0 是基础反射率（非金属通常是 0.04，金属是它本身的颜色）
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// =====================================================================

void main() {
    // 1. 提取基础材质属性
    // (由于我们在 C++ 中创建贴图用的是 SRGB 格式，Vulkan 采样时会自动转回 Linear 空间，非常完美)
    vec3 albedo = texture(BaseColorMap, inUV).rgb * params.BaseColorTint.rgb;
    float metallic = params.Metallic;
    float roughness = params.Roughness;

    // 2. 计算 PBR 所需的基础向量 (极其重要，全部要 normalize)
    vec3 N = normalize(inNormal);
    vec3 V = normalize(scene.CameraPos.xyz - inWorldPos);
    vec3 L = normalize(scene.LightPos.xyz - inWorldPos);
    vec3 H = normalize(V + L); // 半程向量

    // 3. 计算基础反射率 F0
    // 非金属绝大部分是 0.04，如果是金属，则使用其本身的颜色 (Albedo) 作为高光颜色
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // 4. 计算光线辐射度 (Radiance) 与平方反比衰减
    float distance = length(scene.LightPos.xyz - inWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = scene.LightColor.rgb * scene.LightColor.a * attenuation;

    // --- 开始组装 Cook-Torrance BRDF ---

    // 调用你手撕的三个函数
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // 组装高光分子与分母
    vec3 numerator    = NDF * G * F;
    // 加 0.0001 是为了防止除以 0 导致屏幕出现黑斑
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    // 能量守恒：反射的光(kS) + 折射的光(kD) = 1.0
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // 纯金属不发生漫反射，强制压为 0

    // 计算这盏灯对像素的最终贡献
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // --- 结束 Cook-Torrance BRDF ---

    // 5. 加上一个极弱的环境光，防止背光面彻底变成死黑
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // 6. HDR 色调映射 (Tone Mapping)
    // 现实中的光强可以远大于 1.0，但屏幕只能显示 0~1。我们要把过曝的颜色压回来 (Reinhard Tone Mapping)
    color = color / (color + vec3(1.0));

    // 7. Gamma 校正？
    // 注意：因为你在 C++ 的 Swapchain 里申请的是 VK_FORMAT_B8G8R8A8_SRGB，
    // Vulkan 硬件会自动在最后一步执行 Linear -> sRGB 的转换！
    // 所以这里【千万不要】再手动 pow(color, 1.0/2.2) 了，否则画面会发灰变白。

    outFragColor = vec4(color, 1.0);
}