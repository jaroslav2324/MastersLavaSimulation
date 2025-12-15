#include "CommonData.hlsl"

StructuredBuffer<float3> predictedPositionBuffer : register(t1);

RWStructuredBuffer<uint> hashBuffer  : register(u8);
RWStructuredBuffer<uint> indexBuffer : register(u9);

[numthreads(256, 1, 1)]
void CS_HashParticles(uint id : SV_DispatchThreadID)
{
    if (id >= numParticles) return;

    float3 pos = predictedPositionBuffer[id];

    // Convert world position to grid cell coordinates
    int3 cell = int3(floor((pos - worldOrigin) / cellSize));

    // Clamp to grid to avoid out-of-range access
    cell = clamp(cell, int3(0,0,0), int3(gridResolution) - 1);

    // Convert (x,y,z) to linear cell index
    uint hashValue =
        (uint(cell.z) * gridResolution.y + uint(cell.y)) *
         gridResolution.x + uint(cell.x);

    hashBuffer[id]  = hashValue;
    indexBuffer[id] = id;
}