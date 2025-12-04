#include "CommonSimulationKernels.hlsl"

StructuredBuffer<float3> predictedPositions : register(t0);

StructuredBuffer<uint> sortedParticleIndecies : register(t1);
StructuredBuffer<uint> cellStart : register(t2);
StructuredBuffer<uint> cellEnd   : register(t3);

RWStructuredBuffer<float> density     : register(u0);
RWStructuredBuffer<float> constraintC : register(u1);

cbuffer GridParams : register(b1)
{
    int3  gridResolution;
    float cellSize;
    float h;
    float h2;
    float mass;
    float rho0;
};

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;
    float3 qi = predictedPositions[i];

    int3 cell = int3(floor(qi / cellSize));

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

        // TODO: use general function
        uint hash =
            ncell.x +
            ncell.y * gridResolution.x +
            ncell.z * gridResolution.x * gridResolution.y;

        uint start = cellStart[hash];
        uint end   = cellEnd[hash];
        //if (start == 0xFFFFFFFF) continue;

        [loop]
        for (uint idx = start; idx < end; idx++)
        {
            uint j = sortedParticleIndecies[idx];
            float3 qj = predictedPositions[j];

            float3 r = qi - qj;
            float dist2 = dot(r,r);
            if (dist2 >= h2) continue;

            // TODO: constant mass 1
            rho += mass * cubic_kernel_height(r);
        }
    }


    density[i] = rho;
    constraintC[i] = rho / rho0 - 1.0;
}