#version 450

// ==========================================
// 顶点输入 (与 FMesh 和 PipelineBuilder 中的 Layout 严格对应)
// ==========================================
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal; // 之前跳过了，现在 PBR 需要用到！
layout(location = 2) in vec2 inUV;

// ==========================================
// 顶点输出 (传给 Fragment Shader)
// ==========================================
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;

// ==========================================
// Push Constants (小数据，通常放 MVP 矩阵)
// ==========================================
layout(push_constant) uniform PushConstants {
    mat4 render_matrix; // Model-View-Projection 矩阵
// 注意：未来 PBR 还需要传入 Model 矩阵用来计算 WorldPos 和转换法线
// 目前为了先把框架跑通，我们暂时只用 render_matrix
} constants;

void main() {
    // 最终屏幕裁剪坐标
    gl_Position = constants.render_matrix * vec4(inPosition, 1.0);

    // 透传基础数据
    outUV = inUV;

    // TODO: 正确的做法是用 Model 矩阵和逆转置矩阵来计算以下两个值
    // 现在先随便传一下占个位，保证编译通过
    outNormal = inNormal;
    outWorldPos = inPosition;
}