struct VSInput
{
    float3 position;
    float3 color;
};

struct VSOutput
{
    float4 position: SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vmain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fmain(VSOutput vin)
{
    float3 color = vin.color;
    return float4(color, 1.0);
}
