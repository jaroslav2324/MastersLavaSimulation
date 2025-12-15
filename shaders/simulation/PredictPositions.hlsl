#include "CommonData.hlsl"

StructuredBuffer<float3> gPositionsSrc     : register(t0); // (x, y, z, radius or padding)
StructuredBuffer<float3> gVelocitySrc      : register(t2); // (vx, vy, vz, pad)

RWStructuredBuffer<float3> gPredictedPositionsDst : register(u0);
RWStructuredBuffer<float3> gVelocityDst           : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x; // TODO: точно правильный индекс?
    if (idx >= numParticles) return;

    float3 pos = gPositionsSrc[idx];
    float3 vel = gVelocitySrc[idx];

    // Apply external forces
    vel.xyz = vel + gravityVec * dt;

    // Predict new position
    float3 posPred = pos + vel * dt;

    gVelocityDst[idx] = vel;
    gPredictedPositionsDst[idx] = posPred;
}