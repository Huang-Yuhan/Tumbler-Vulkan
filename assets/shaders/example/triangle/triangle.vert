#version 450

// 输入
layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inNormal; // 暂时不用，但在内存里是存在的
layout(location = 2) in vec2 inUV;       // 【新增】读取 UV

// 输出
layout(location = 0) out vec2 outUV;     // 【新增】传给 Fragment Shader

// Push Constants
layout(push_constant) uniform PushConstants {
    mat4 render_matrix;
} constants;

void main() {
    // 变换坐标
    gl_Position = constants.render_matrix * vec4(inPosition, 1.0);

    // 传递 UV
    outUV = inUV;
}