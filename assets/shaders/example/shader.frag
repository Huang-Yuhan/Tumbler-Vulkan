#version 450

// 从顶点着色器接收颜色
layout(location = 0) in vec3 fragColor;

// 输出最终颜色到屏幕（Location 0 对应交换链图像）
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}