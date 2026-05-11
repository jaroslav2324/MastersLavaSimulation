#include "CommonData.hlsl"

StructuredBuffer<float3> gPositionsSrc    : register(t0);
StructuredBuffer<uint>   particleIndices  : register(t4);

RWStructuredBuffer<float3> gPredictedPositionsDst : register(u7);
RWStructuredBuffer<float3> gVelocity              : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint tid : SV_DispatchThreadID)
{
    if (tid >= numParticles) return;
    uint idx = particleIndices[tid];

    float3 pos = gPositionsSrc[idx];
    float3 vel = gVelocity[idx];

    vel = vel + gravityVec * dt;

    gVelocity[idx] = vel;
    gPredictedPositionsDst[idx] = pos + vel * dt;
}
