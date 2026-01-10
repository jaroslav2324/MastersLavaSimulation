// #5
#include "CommonKernels.hlsl"

StructuredBuffer<float3> predictedPositions : register(t7);

StructuredBuffer<uint> particleIndecies : register(t4);
StructuredBuffer<uint> cellStart : register(t5);
StructuredBuffer<uint> cellEnd   : register(t6);

RWStructuredBuffer<float> density     : register(u8);
RWStructuredBuffer<float> constraintC : register(u9);

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndecies[gid];
    float3 qi = predictedPositions[i];

    int3 cell = GetCellCoord(qi);

    float rho = 0.0;

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        int3 ncell = cell + int3(dx,dy,dz);

        if (ncell.x < 0 || ncell.y < 0 || ncell.z < 0 ||
            ncell.x >= gridResolution.x ||
            ncell.y >= gridResolution.y ||
            ncell.z >= gridResolution.z)
            continue;

        uint hash = GetCellHash(ncell);

        uint start = cellStart[hash];
        uint end   = cellEnd[hash];

        [loop]
        for (uint idx = start; idx < end; idx++)
        {
            uint j = particleIndecies[idx];
            // do not skip if i == j

            float3 qj = predictedPositions[j];

            float3 r = qi - qj;
            float dist2 = dot(r,r);
            if (dist2 >= h2) continue;

            rho += mass * cubic_kernel_height(r);
        }
    }


    density[i] = rho;
    constraintC[i] = rho / rho0 - 1.0;
}