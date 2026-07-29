// GPU-driven instanced cluster rendering.
// Each instance is one Nanite cluster. Vertex data and indices live in SSBOs.
// No vertex input bindings — the shader reads everything from buffers.

struct ClusterInfo
{
    uint   indexOffset;   // first index in the global index buffer
    uint   vertexOffset;  // first vertex in the global position buffer
    uint   indexCount;    // indices in this cluster (= kClusterTriangleCount * 3)
    uint   pad;
    float4 color;         // debug per-cluster color
};

struct PushConstants
{
    float4x4 viewProj;
    uint     wireframe;
};
[[vk::push_constant]] PushConstants pc;

// ---- Set 0 bindings ----
[[vk::binding(0, 0)]] StructuredBuffer<ClusterInfo> clusterData;
[[vk::binding(1, 0)]] StructuredBuffer<uint>        clusterIndices;   // packed per-cluster indices
[[vk::binding(2, 0)]] StructuredBuffer<float>       clusterPositions; // xyz per vertex, 3 floats per vertex

// ---- Vertex / Fragment IO ----
struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

[shader("vertex")]
VSOutput vert_main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    ClusterInfo ci = clusterData[instanceID];
    uint localIdx  = ci.indexOffset + vertexID;
    uint globalIdx = clusterIndices[localIdx];
    uint posBase   = ci.vertexOffset + globalIdx;

    float x = clusterPositions[posBase * 3 + 0];
    float y = clusterPositions[posBase * 3 + 1];
    float z = clusterPositions[posBase * 3 + 2];

    VSOutput o;
    o.position = mul(pc.viewProj, float4(x, y, z, 1.0));
    o.color    = ci.color.rgb;
    return o;
}

[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target0
{
    if (pc.wireframe != 0)
        return float4(0.0, 0.0, 0.0, 1.0);
    return float4(input.color, 1.0);
}
