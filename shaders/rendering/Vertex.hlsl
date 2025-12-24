
cbuffer Globals : register(b0)
{
    float4x4 view;
    float4x4 invView;
    float4x4 proj;
    float nearPlane;
    float farPlane;
    float particleRadius;
    float1 _pad;
};

StructuredBuffer<float3> particlePositions : register(t0);
StructuredBuffer<float>  particleTemperature : register(t1);

struct VSOut
{
    float4 posH : SV_Position;    
    float2 uv   : TEXCOORD0;        
    float  temp : TEXCOORD1;      
};

VSOut VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOut o;

    float3 center = particlePositions[instanceID];
    o.temp   = particleTemperature[instanceID];

    static const float2 corners[4] =
    {
        float2(-1, -1),
        float2(-1,  1),
        float2( 1, -1),
        float2( 1,  1)
    };

    float2 corner = corners[vertexID % 4];
    o.uv = corner;

    float3 camRight = normalize(view[0].xyz);
    float3 camUp    = normalize(view[1].xyz);

    float3 worldPos =
        center +
        camRight * (corner.x * particleRadius) +
        camUp    * (corner.y * particleRadius);

    float4 posV = mul(view, float4(worldPos, 1.0));
    o.posH = mul(proj, posV);

    return o;
}