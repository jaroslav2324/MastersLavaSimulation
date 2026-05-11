#include "CommonData.hlsl"

StructuredBuffer<uint> particleIndices : register(t4);

RWStructuredBuffer<float3> gPredictedPositions : register(u7);
RWStructuredBuffer<float3> gVelocity           : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
}
