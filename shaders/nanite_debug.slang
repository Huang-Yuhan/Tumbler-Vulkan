// Nanite debug shader — instanced GPU-driven cluster rendering
//
// Vertex shader reads cluster metadata + vertex/index data from SSBOs.
// MVP matrix comes via push constant.
// Fragment shader passes through deterministic per-cluster color.

struct ClusterInfo
{
    uint vertexOffset;   // first vertex index in the global position buffer
    uint indexOffset;    // first index in the global index buffer
    uint triangleCount;  // triangles in this cluster
};

// ---- Set 0 bindings ----
[[vk::binding(0, 0)]] StructuredBuffer<ClusterInfo> clusterData;
[[vk::binding(1, 0)]] StructuredBuffer<float>      positions;  // 3 floats per vertex
[[vk::binding(2, 0)]] ByteAddressBuffer             indices;

// ---- Push constants ----
struct PushConstants
{
    float4x4 viewProj;
};
[[vk::push_constant]] PushConstants pc;

// ---- Shared IO ----
struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

// ---- Vertex Shader ----
[shader("vertex")]
VSOutput vert_main(
    uint vertexID   : SV_VertexID,
    uint instanceID : SV_InstanceID)
{
    // DEBUG: full-screen triangle from vertex ID mod 3
    float x = (vertexID % 3 == 0) ? -1.0 : ((vertexID % 3 == 1) ? 3.0 : -1.0);
    float y = (vertexID % 3 == 0) ? -1.0 : ((vertexID % 3 == 1) ? -1.0 : 3.0);

    VSOutput o;
    o.position = float4(x, y, 0.0, 1.0);
    o.color    = float3(float(instanceID) / 32.0, 0.5, 0.0);
    return o;
}

// ---- Fragment Shader ----
[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target0
{
    return float4(1.0, 0.5, 0.0, 1.0);  // DEBUG: bright orange
}
