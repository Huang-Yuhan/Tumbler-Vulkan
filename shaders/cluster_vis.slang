// Cluster visualization shader — simple vertex-attribute passthrough
// Position + Color vertex attributes, viewProj push constant.

struct VSInput
{
    float3 position : POSITION;
    float3 color    : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

struct PushConstants
{
    float4x4 viewProj;
    uint     wireframe;
};
[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
VSOutput vert_main(VSInput input)
{
    VSOutput o;
    o.position = mul(pc.viewProj, float4(input.position, 1.0));
    o.color    = input.color;
    return o;
}

[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target0
{
    if (pc.wireframe != 0)
        return float4(0.0, 0.0, 0.0, 1.0);
    return float4(input.color, 1.0);
}
