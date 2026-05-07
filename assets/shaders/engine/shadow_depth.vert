#version 450
// Shadow depth pass vertex shader

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform ShadowUBO {
    mat4 LightViewProj;
} ubo;

void main() {
    gl_Position = ubo.LightViewProj * vec4(inPosition, 1.0);
}
