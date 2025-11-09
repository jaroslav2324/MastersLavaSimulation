// Globals: view/proj
cbuffer Globals : register(b0)
{
    matrix view;
    matrix proj;
    float nearPlane;
    float farPlane;
    float2 pad;
};

// Per-object: model matrix
cbuffer PerObject : register(b1)
{
    matrix model;
};

// Vertex input (position as float4 to match CPU side)
struct VSInput
{
    float4 position : POSITION;
};

// Vertex->Pixel output
struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

// Vertex shader: transform position by model, view, proj
PSInput VSMain(VSInput vin)
{
    PSInput p;
    float4 worldPos = mul(model, vin.position);
    float4 viewPos = mul(view, worldPos);
    p.position = mul(proj, viewPos);
    p.worldPos = worldPos.xyz;
    return p;
}

// Pixel shader: simple shading (replace with phong/lambert if you add normals)
float4 PSMain(PSInput pin) : SV_Target
{
    float t = saturate((pin.worldPos.y + 1.0f) * 0.5f);
    float3 baseColor = lerp(float3(0.3f, 0.5f, 0.9f), float3(0.9f, 0.7f, 0.3f), t);
    return float4(baseColor, 1.0f);
}
