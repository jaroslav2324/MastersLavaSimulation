// #8
#include "CommonData.hlsl"

StructuredBuffer<uint> particleIndices : register(t4);

RWStructuredBuffer<float3> predicted : register(u7); // q_i*
RWStructuredBuffer<float3> deltaP    : register(u11); // Δp_i

// TODO: move to ComputeDeltaPos.hlsl?
[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;

    uint i = particleIndices[gid];

    float3 dp = deltaP[i];

    predicted[i] += dp;
    deltaP[i] = float3(0, 0, 0);
}