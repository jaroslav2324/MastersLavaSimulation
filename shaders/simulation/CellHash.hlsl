#include "CommonKernels.hlsl"

StructuredBuffer<float3> predictedPositionBuffer : register(t1);

RWStructuredBuffer<uint> hashBuffer  : register(u3);
RWStructuredBuffer<uint> indexBuffer : register(u4);

[numthreads(256, 1, 1)]
void CS_HashParticles(uint id : SV_DispatchThreadID)
{
    if (id >= numParticles) return;

    float3 pos = predictedPositionBuffer[id];
    uint3 cell = GetCellCoord(pos);

    hashBuffer[id]  = GetCellHash(cell);
    indexBuffer[id] = id;
}