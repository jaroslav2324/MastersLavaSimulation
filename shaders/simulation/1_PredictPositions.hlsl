// #1
#include "CommonData.hlsl"

StructuredBuffer<float3> gPositionsSrc   : register(t0);
StructuredBuffer<uint>   particleIndices : register(t4);
StructuredBuffer<uint>   gPhase          : register(t14);

RWStructuredBuffer<float3> gPredictedPositionsDst : register(u7);
RWStructuredBuffer<float3> gVelocity              : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint tid : SV_DispatchThreadID)
{
    if (tid >= numParticles) return;
    uint idx = particleIndices[tid];

    float3 pos = gPositionsSrc[idx];

    // Solid particles are static obstacles — they don't move
    if (gPhase[idx] == 1u)
    {
        gVelocity[idx]            = float3(0, 0, 0);
        gPredictedPositionsDst[idx] = pos;
        return;
    }

    float3 vel = gVelocity[idx];
    vel += gravityVec * dt;

    float3 posPred = pos + vel * dt;

    gVelocity[idx]              = vel;
    gPredictedPositionsDst[idx] = posPred;
}
