cbuffer Globals : register(b0)
{
    float4x4 view;
    float4x4 invView;
    float4x4 proj;
    float3 globalLightDirection;  // world-space direction light travels, normalized
    float nearPlane;
    float farPlane;
    float particleRadius;
    float _pad;
};

struct VSOut
{
    float4 posH      : SV_Position;
    float2 uv        : TEXCOORD0;
    float  temp      : TEXCOORD1;
    float3 billRight : TEXCOORD2;
    float3 billUp    : TEXCOORD3;
    float3 billFwd   : TEXCOORD4;
};

static const uint LavaLUTSize = 8;
static const float3 LavaColorLUT[LavaLUTSize] =
{
    float3(0.02, 0.02, 0.02), // ~500K  почти чёрный
    float3(0.15, 0.02, 0.02), // тёмно-красный
    float3(0.35, 0.05, 0.02), // бордовый
    float3(0.65, 0.10, 0.02), // красно-оранжевый
    float3(0.90, 0.30, 0.05), // оранжевый
    float3(1.00, 0.60, 0.10), // жёлто-оранжевый
    float3(1.00, 0.85, 0.30), // жёлтый
    float3(1.00, 1.00, 0.80)  // ~1500K почти белый
};

static const float Tmin = 500.0;
static const float Tmax = 1500.0;
float3 TempToLavaColor(float temperatureK)
{
    float t = clamp(temperatureK, Tmin, Tmax);
    float u = (t - Tmin) / (Tmax - Tmin);

    float fIndex = u * (LavaLUTSize - 1);

    uint index0 = (uint)floor(fIndex);
    uint index1 = min(index0 + 1, LavaLUTSize - 1);

    float frac = fIndex - index0;

    return lerp(LavaColorLUT[index0], LavaColorLUT[index1], frac);
}

float4 PSMain(VSOut i) : SV_Target
{
    float r2 = dot(i.uv, i.uv);
    clip(1.0 - r2);

    // Sphere normal in world space: billboard axes are fixed to camera position,
    // so this normal is independent of camera view direction.
    float nz = sqrt(1.0 - r2);
    float3 N = normalize(i.billRight * i.uv.x + i.billUp * i.uv.y + i.billFwd * nz);

    float3 L      = -normalize(globalLightDirection);  // direction from surface toward light
    float ndotl   = max(dot(N, L), 0.0);
    float ambient = 0.15;
    float lighting = ambient + (1.0 - ambient) * ndotl;

    float3 col = TempToLavaColor(i.temp) * lighting;
    return float4(col, 1.0);
}
