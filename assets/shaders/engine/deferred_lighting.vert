#version 450

layout(location = 0) out vec2 outUV;

void main() {
    // 自动抛出一个足以覆盖全屏的巨大三角形，从而不需要绑定任何 Vertex Buffer
    // Vertex ID: 0 -> UV: (0, 0)
    // Vertex ID: 1 -> UV: (2, 0)
    // Vertex ID: 2 -> UV: (0, 2)
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    // 从 Vulkan 坐标系 (0-1) 映射到 Clip Space (-1 到 1)
    gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
