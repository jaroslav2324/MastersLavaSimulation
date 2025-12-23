#include "CommonData.hlsl"

StructuredBuffer<uint> particleIndices : register(t4);

RWStructuredBuffer<float3> gPredictedPositions : register(u1);
RWStructuredBuffer<float3> gVelocity           : register(u2);

[numthreads(256, 1, 1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid.x];

    float3 q = gPredictedPositions[i];
    float3 v = gVelocity[i];

    if (q.y < 0.0)
    {
        q.y = 0.0;

        if (v.y < 0.0)
        {
            v.y = 0.0;
            gVelocity[i] = v;
        }
        gPredictedPositions[i] = q;
    }
}