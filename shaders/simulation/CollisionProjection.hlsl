#include "CommonData.hlsl"

RWStructuredBuffer<float3> gPredictedPositions : register(u0);
RWStructuredBuffer<float3> gVelocity           : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;
    if (i >= numParticles) return;

    float3 q = gPredictedPositions[i];
    float3 v = gVelocity[i];

    if (q.y < 0.0)
    {
        q.y = 0.0;

        if (v.y < 0.0)
        {
            v.y = 0.0;
        }
    }

    gPredictedPositions[i] = q;
    gVelocity[i] = v;
}