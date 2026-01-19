#version 450

// 1. 输入：从 Vertex Shader 传过来的颜色（如果有的话）
layout(location = 0) in vec3 fragColor;



// 2. 输出：必须显式声明！Location 0 对应 Swapchain 的第一个附件
layout(location = 0) out vec4 outColor;

void main() {
    // 3. 赋值给输出变量，而不是 gl_FragColor
    // 这里输出红色 (R=1, G=0, B=0, A=1)
    outColor = vec4(fragColor, 1.0);
}