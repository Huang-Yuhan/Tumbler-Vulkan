#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in mat3 inTBN;

// G-Buffer 输出 (对应 FDeferredPipeline 的附件 1 和 2)
layout(location = 0) out vec4 outAlbedo; // RGB: BaseColor, A: Metallic
layout(location = 1) out vec4 outNormal; // RGB: Normal,    A: Roughness

layout(set = 1, binding = 0) uniform sampler2D BaseColorMap;
layout(set = 1, binding = 1) uniform sampler2D NormalMap;

layout(set = 1, binding = 2) uniform MaterialParams {
    vec4  BaseColorTint;
    float Roughness;
    float Metallic;
    float NormalMapStrength;
    int   TwoSided;
} params;

// 我们不需要 SceneData (set=0) 计算光照，仅为输出材质几何属性
// 但是如果不定义它，绑定了 set=0 也不会报错

void main() {
    // 1. 采样基础颜色与材质属性
    vec3 albedo = texture(BaseColorMap, inUV).rgb * params.BaseColorTint.rgb;
    float metallic = params.Metallic;
    float roughness = params.Roughness;
    float normalStrength = params.NormalMapStrength;

    // 2. 解析法线
    vec3 sampledNormal = texture(NormalMap, inUV).xyz;
    vec3 N;
    
    // 检查是否是默认白色贴图
    if (length(sampledNormal - vec3(1.0)) < 0.01) {
        N = normalize(inNormal);
    } else {
        vec3 tangentNormal = sampledNormal * 2.0 - 1.0;
        tangentNormal.xy *= normalStrength;
        N = normalize(inTBN * tangentNormal);
    }
    
    // 注意：双面材质的光照表面翻转 (faceforward) 需要视线 V，这在光照阶段才会做。
    // 在这里我们只输出世界坐标下的法线。

    // 3. 将数据打包进 G-Buffer
    outAlbedo = vec4(albedo, metallic);
    outNormal = vec4(N, roughness);
}
