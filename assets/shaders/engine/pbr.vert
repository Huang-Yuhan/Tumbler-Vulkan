#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;

// 【新增】Binding 0: 全局场景参数
layout(set = 0, binding = 0) uniform SceneData {
    mat4 ViewProj;
    vec4 CameraPos;
    vec4 LightPos;
    vec4 LightColor;
} scene;

// 【修改】Push Constants 现在只接收 Model 矩阵
layout(push_constant) uniform PushConstants {
    mat4 ModelMatrix;
} constants;

void main() {
    // 1. 计算世界坐标
    vec4 worldPos = constants.ModelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    // 2. 结合 Set 0 里的 ViewProj 计算裁剪坐标
    gl_Position = scene.ViewProj * worldPos;

    outUV = inUV;

    // 3. 计算法线 (目前先简单乘一下 Model，严格来说应该用逆转置矩阵)
    outNormal = mat3(constants.ModelMatrix) * inNormal;
}