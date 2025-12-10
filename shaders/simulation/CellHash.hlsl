#include "CommonData.hlsl"

StructuredBuffer<float3> positionBuffer : register(t0);

RWStructuredBuffer<uint> hashBuffer  : register(u0);
RWStructuredBuffer<uint> indexBuffer : register(u1);

[numthreads(256, 1, 1)]
void CS_HashParticles(uint id : SV_DispatchThreadID)
{
    if (id >= numParticles) return;

    float3 pos = positionBuffer[id];

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