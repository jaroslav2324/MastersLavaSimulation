// #3
#include "CommonKernels.hlsl"

StructuredBuffer<float3> predictedPositionBuffer : register(t7);

RWStructuredBuffer<uint> hashBuffer  : register(u3);
RWStructuredBuffer<uint> indexBuffer : register(u4);

[numthreads(256, 1, 1)]
void CS_HashParticles(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint id = indexBuffer[gid];

    float3 pos = predictedPositionBuffer[id];
    uint3 cell = GetCellCoord(pos);

    hashBuffer[id]  = GetCellHash(cell);
    indexBuffer[id] = id;
}