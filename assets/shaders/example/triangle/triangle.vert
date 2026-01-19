#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragColor;

// 【新增】Push Constant 数据块
// 就像一个结构体，CPU 会把数据推送到这里
layout(push_constant) uniform PushConstants {
    mat4 render_matrix; // 变换矩阵 (MVP)
} constants;

void main() {
    // 矩阵乘法：Matrix * Vector
    // 这将把顶点从模型空间变换到裁剪空间
    gl_Position = constants.render_matrix * vec4(inPosition, 1.0);

    // 简单的颜色调试
    fragColor = vec3(1.0, 0.0, 0.0);
}