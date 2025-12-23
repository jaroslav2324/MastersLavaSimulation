#include "CommonKernels.hlsl"

StructuredBuffer<float3> predicted : register(t1);
StructuredBuffer<uint>   particleIndices : register(t4);
StructuredBuffer<uint>   cellStart : register(t5);
StructuredBuffer<uint>   cellEnd   : register(t6);
StructuredBuffer<float>  viscCoeff : register(t13);

RWStructuredBuffer<float3> velocities : register(u2);

// TODO: check
[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid];

    float3 xi = predicted[i];
    float3 vi = velocities[i];
    float ci  = viscCoeff[i];

    if (ci <= 0.0)
    {
        velocities[i] = vi;
        return;
    }

    float3 dv = float3(0,0,0);

    uint3 cell = GetCellCoord(xi);

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        int3 nc = int3(cell) + int3(dx,dy,dz);

        if (nc.x < 0 || nc.y < 0 || nc.z < 0 ||
            nc.x >= gridResolution.x ||
            nc.y >= gridResolution.y ||
            nc.z >= gridResolution.z)
        {
            continue;
        }

        uint hash = GetCellHash(uint3(nc));
        uint start = cellStart[hash];
        uint end   = cellEnd[hash];

        for (uint idx = start; idx < end; idx++)
        {
            uint j = particleIndices[idx];
            if (j == i) continue;

            float3 xj = predicted[j];
            float3 rij = xi - xj;
            float r2 = dot(rij, rij);

            if (r2 >= h2) continue;

            float W = cubic_kernel_height(rij);
            dv += (velocities[j] - vi) * W;
        }
    }

    velocities[i] = vi + ci * dv;
}