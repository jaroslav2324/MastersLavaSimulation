// ApplyDeltaPosCS.hlsl
#include "CommonData.hlsl"

RWStructuredBuffer<float3> predicted : register(u0); // q_i*
RWStructuredBuffer<float3> deltaP    : register(u2); // Δp_i

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;

    float3 dp = deltaP[i];

    // Apply PBF correction
    predicted[i] += dp;

    // Clear for next iteration
    deltaP[i] = float3(0, 0, 0);
}