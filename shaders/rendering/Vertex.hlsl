cbuffer CameraBuffer : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

StructuredBuffer<float3> particlePositions : register(t0);
StructuredBuffer<float>  particleTemperature : register(t1);

cbuffer RenderParams : register(b1)
{
    float particleRadius; // in world units
}


struct VSOut
{
    float4 posH : SV_Position;    
    float2 uv   : UV;             // quad UV: (-1..1)
    float  temp : TEMP;   // TODO: who is TEMP?        
};

// TODO: CPU DrawInstanced(4, particleCount, 0, 0)
VSOut VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOut o;

    float3 center = particlePositions[instanceID];
    float  temp   = particleTemperature[instanceID];

    static const float2 corners[4] =
    {
        float2(-1, -1),
        float2(-1,  1),
        float2( 1, -1),
        float2( 1,  1)
    };

    float2 corner = corners[vertexID % 4];
    o.uv = corner;

    // Billboard: transform quad in camera space
    // Extract camera right & up from view matrix
    float3 camRight = float3(view._11, view._21, view._31);
    float3 camUp    = float3(view._12, view._22, view._32);

    float worldSize = particleRadius;

    float3 worldPos =
        center +
        camRight * (corner.x * worldSize) +
        camUp    * (corner.y * worldSize);

    float4 posV = mul(view, float4(worldPos, 1.0));
    o.posH = mul(proj, posV);

    o.temp = temp;

    return o;
}