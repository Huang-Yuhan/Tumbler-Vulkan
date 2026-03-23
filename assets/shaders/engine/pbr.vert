#version 450

// ==========================================
// 顶点输入
// ==========================================
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;

// ==========================================
// 传给 Fragment Shader 的数据
// ==========================================
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out mat3 outTBN;

// ==========================================
// 描述符与常量
// ==========================================
layout(set = 0, binding = 0) uniform SceneData {
    mat4 ViewProj;
    vec4 CameraPos;
    vec4 LightPos;
    vec4 LightColor;
} scene;

layout(push_constant) uniform PushConstants {
    mat4 ModelMatrix;
} constants;

void main() {
    // 1. 计算世界坐标
    vec4 worldPos = constants.ModelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    // 2. 计算裁剪坐标
    gl_Position = scene.ViewProj * worldPos;

    // 3. 透传 UV
    outUV = inUV;

    // 4. 计算正确的法线矩阵 (逆转置矩阵)
    mat3 normalMatrix = transpose(inverse(mat3(constants.ModelMatrix)));
    
    // 5. 计算 TBN (Tangent-Bitangent-Normal) 矩阵
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    
    // 正交化 T 相对于 N
    T = normalize(T - dot(T, N) * N);
    
    // 计算 Bitangent
    vec3 B = cross(N, T);
    
    outTBN = mat3(T, B, N);
    
    // 也传递世界空间的法线作为备份
    outNormal = N;
}
