
cbuffer Globals : register(b0)
{
    float4x4 view;
    float4x4 invView;
    float4x4 proj;
    float3 globalLightDirection; 
    float nearPlane;
    float farPlane;
    float particleRadius;
    float _pad;
};

StructuredBuffer<float3> particlePositions : register(t0);
StructuredBuffer<float>  particleTemperature : register(t1);

struct VSOut
{
    float4 posH      : SV_Position;
    float2 uv        : TEXCOORD0;
    float  temp      : TEXCOORD1;
    float3 billRight : TEXCOORD2;
    float3 billUp    : TEXCOORD3;
    float3 billFwd   : TEXCOORD4;  
};

VSOut VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOut o;

    float3 center = particlePositions[instanceID];
    o.temp = particleTemperature[instanceID];

    static const float2 corners[4] =
    {
        float2(-1, -1),
        float2(-1,  1),
        float2( 1, -1),
        float2( 1,  1)
    };

    float2 corner = corners[vertexID % 4];
    o.uv = corner;

    float3 cameraPos = float3(invView[0][3], invView[1][3], invView[2][3]);
    float3 toCamera  = normalize(cameraPos - center);

    float3 worldUp = float3(0, 1, 0);
    float3 billRight, billUp;
    if (abs(dot(toCamera, worldUp)) < 0.999)
    {
        billRight = normalize(cross(worldUp, toCamera));
        billUp    = cross(toCamera, billRight);
    }
    else
    {
        billRight = float3(1, 0, 0);
        billUp    = normalize(cross(toCamera, billRight));
    }

    o.billRight = billRight;
    o.billUp    = billUp;
    o.billFwd   = toCamera;

    float3 worldPos =
        center +
        billRight * (corner.x * particleRadius) +
        billUp    * (corner.y * particleRadius);

    float4 posV = mul(view, float4(worldPos, 1.0));
    o.posH = mul(proj, posV);

    return o;
}
