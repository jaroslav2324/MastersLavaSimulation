
#include "CommonData.hlsl"

uint3 GetCellCoord(float3 p)
{
    return uint3(
        clamp((uint)floor((p.x - worldOrigin.x) / cellSize), 0, gridResolution.x - 1),
        clamp((uint)floor((p.y - worldOrigin.y) / cellSize), 0, gridResolution.y - 1),
        clamp((uint)floor((p.z - worldOrigin.z) / cellSize), 0, gridResolution.z - 1)
    );
}

uint GetCellHash(uint3 c)
{
    return c.x 
         + c.y * gridResolution.x
         + c.z * gridResolution.x * gridResolution.y;
}



float GetThermalConductivity(float T)
{
    const float kTemperatureSolid = 900; // TODO: check
    const float kTemperatureLiquid = 1200;
    const float kConductivitySolid = 2.0;
    const float kConductivityLiquid = 2.5;
    float a = saturate((T - kTemperatureSolid) / (kTemperatureLiquid - kTemperatureSolid));
    return lerp(kConductivitySolid, kConductivityLiquid, a);
}

static const float PI = 3.14159265359;

float poly6_kernel_height(float3 r)
{
    float r2 = dot(r,r);
    float h2 = h*h;
    if (r2 > h2) return 0.0;
    float t = (h2 - r2);
    // coefficient for 3D poly6: 315 / (64*pi*h^9)
    return 315.0 * t*t*t / (64.0 * PI * pow(h, 9.0));
}

float3 spiky_kernel_gradient(float3 r)
{
    float dist = length(r);
    if (dist > h || dist < 1e-6) return float3(0,0,0);
    // -45/(pi*h^6) * (h - r)^2 * (r / dist)
    float k = -45.0 / (PI * pow(h,6.0));
    float f = k * (h - dist)*(h - dist);
    return f * (r / dist);
}

float cubic_kernel_height(float3 r)
{
    float dist = length(r);
    if (dist > h) return 0.0;
    float q = dist / h;
    float k = 8.0 / (PI * h*h*h); // cubic spline normalization in 3D
    if (q <= 0.5)
    {
        float q2 = q*q;
        float q3 = q2*q;
        return k * (6.0*q3 - 6.0*q2 + 1.0);
    }
    float t = 1.0 - q;
    return k * (2.0 * t*t*t);
}

float3 cubic_kernel_gradient(float3 r)
{
    float dist = length(r);
    if (dist > h || dist < 1e-6) return float3(0,0,0);
    float q = dist / h;
    float invDist = 1.0 / (dist * h);
    float3 gradq = r * invDist; // d(q)/d(r) * r/|r| => r/(dist*h)
    float l = 48.0 / (PI * h*h*h);
    if (q <= 0.5)
    {
        return l * q * (3.0*q - 2.0) * gradq;
    }
    float factor = 1.0 - q;
    return l * (-factor * factor) * gradq;
}
