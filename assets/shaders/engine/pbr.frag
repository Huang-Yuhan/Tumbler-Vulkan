#version 450

// ==========================================
// 输入 (来自 Vertex Shader)
// ==========================================
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;

// ==========================================
// 输出 (输出到 Framebuffer)
// ==========================================
layout(location = 0) out vec4 outFragColor;

// ==========================================
// 材质参数绑定 (FMaterialInstance 将提供这些数据)
// ==========================================

// Binding 0: 基础漫反射贴图 (Albedo / Base Color)
layout(set = 0, binding = 0) uniform sampler2D BaseColorMap;

// Binding 1: 材质数值参数 (UBO)
// 注意：std140 内存对齐规则！
layout(set = 0, binding = 1) uniform MaterialParams {
    vec4  BaseColorTint; // 基础色染色 (16 bytes)
    float Roughness;     // 粗糙度 (4 bytes)
    float Metallic;      // 金属度 (4 bytes)
    vec2  Padding;       // 凑齐 16 字节对齐 (8 bytes)
} params;

// TODO: 未来还会加入法线贴图 (NormalMap), ORM 贴图等
// layout(set = 0, binding = 2) uniform sampler2D NormalMap;

void main() {
    // 1. 采样基础颜色
    vec4 albedo = texture(BaseColorMap, inUV) * params.BaseColorTint;

    // 2. 提取粗糙度和金属度 (目前没有用上，但确保数据能传过来)
    float roughness = params.Roughness;
    float metallic = params.Metallic;

    // ==========================================
    // [TODO] PBR 光照计算核心 (BRDF, 辐射度量学)
    // ==========================================
    // vec3 N = normalize(inNormal);
    // vec3 V = normalize(CameraPos - inWorldPos);
    // vec3 L = normalize(LightDir);
    // ...
    // vec3 finalColor = Ambient + Lo;

    // 3. 临时输出：为了测试材质框架是否联通，我们先直接输出带染色的 Albedo 颜色
    outFragColor = albedo;
}