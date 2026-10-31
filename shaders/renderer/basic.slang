static const float2 positions[3] = float2[]
(
    float2(-0.5, -0.5),
    float2( 0.5, -0.5),
    float2( 0.0,  0.5),
);

static const float3 colors[3] = float3[]
(
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);

struct VertexInput
{
    uint vid : SV_VertexID;
};

struct VertexOutput
{
    float4 sv_position: SV_Position;
    float3 color;
};

[shader("vertex")]
VertexOutput vmain(VertexInput input)
{
    VertexOutput output;
    output.sv_position = float4(positions[input.vid], 0.0, 1.0);
    output.color = colors[input.vid];
    return output;
}

[shader("fragment")]
float4 fmain(VertexOutput vin) : SV_Target
{
    float3 color = vin.color;
    return float4(color, 1.0);
}
