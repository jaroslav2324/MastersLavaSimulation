#include "CommonData.hlsl"

StructuredBuffer<float3> gPositionsSrc     : register(t0);
StructuredBuffer<uint>   particleIndices  : register(t4);

RWStructuredBuffer<float3> gPredictedPositionsDst : register(u1);
RWStructuredBuffer<float3> gVelocity           : register(u2);

[numthreads(256, 1, 1)]
void CSMain(uint tid : SV_DispatchThreadID)
{
    if (tid >= numParticles) return;
    uint idx = particleIndices[tid]; 

    float3 pos = gPositionsSrc[idx];
    float3 vel = gVelocity[idx];

    // Apply external forces
    vel = vel + gravityVec * dt;

    // Predict new position
    float3 posPred = pos + vel * dt;

    gVelocity[idx] = vel;
    gPredictedPositionsDst[idx] = posPred;
}