#version 450

// 输入：从 Vertex Shader 传来的 UV
layout(location = 0) in vec2 inUV;

// 输出
layout(location = 0) out vec4 outFragColor;

// 【新增】描述符绑定 (Set 0, Binding 0)
// 对应 InitDescriptors 里的设置
layout(set = 0, binding = 0) uniform sampler2D texSampler;

void main() {
    // 采样纹理颜色
    outFragColor = texture(texSampler, inUV);

    // 调试：如果你想把纹理和红色混合，可以写成:
    // outFragColor = texture(texSampler, inUV) * vec4(1.0, 0.0, 0.0, 1.0);
}