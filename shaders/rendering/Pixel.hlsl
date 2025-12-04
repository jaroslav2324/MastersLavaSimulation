struct VSOut
{
    float4 posH : SV_Position;
    float2 uv   : UV;
    float  temp : TEMP;
};

// TODO: replace
// Example temp→color mapping
float3 TempToColor(float t)
{
    // t normalized 0..1 ideally
    // Example gradient: dark red → orange → yellow → white
    float3 c1 = float3(0.3, 0.0, 0.0); // cold
    float3 c2 = float3(1.0, 0.3, 0.0);
    float3 c3 = float3(1.0, 0.9, 0.0);
    float3 c4 = float3(1.0, 1.0, 1.0); // hot

    // Two-stage lerp
    if (t < 0.5)
        return lerp(c1, c2, t * 2.0);
    else
        return lerp(c3, c4, (t - 0.5) * 2.0);
}

float4 PSMain(VSOut i) : SV_Target
{
    // circular mask
    float r = length(i.uv);
    if (r > 1.0)
        discard;

    float t = i.temp;        // 0..1 normalized
    float3 col = TempToColor(t);

    // optional soft falloff on edges for nicer spheres
    float edge = smoothstep(1.0, 0.8, r);

    return float4(col * edge, 1.0);
}