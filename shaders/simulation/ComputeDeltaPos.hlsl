#include "CommonKernels.hlsl"

StructuredBuffer<float3> predicted : register(t1);
StructuredBuffer<uint>   particleIndices : register(t4);
StructuredBuffer<uint>   cellStart : register(t5);
StructuredBuffer<uint>   cellEnd   : register(t6);

StructuredBuffer<float> lambda : register(t10);

RWStructuredBuffer<float3> deltaP : register(u11);


[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid];
    float3 pi = predicted[i];
    float3 dpi = float3(0,0,0);

    // λ_i
    float li = lambda[i];

    int3 cell = GetCellCoord(pi);

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

        //if (start == 0xFFFFFFFF) continue;

        [loop]
        for (uint idx = start; idx < end; idx++)
        {
            uint j = particleIndices[idx];
            if (j == i) continue;

            float3 pj = predicted[j];
            float3 rij = pi - pj;

            float dist2 = dot(rij, rij);
            if (dist2 >= h2) continue;

            float3 gradW = cubic_kernel_gradient(rij);

            float lj = lambda[j];

            dpi += (li + lj) * gradW;
        }
    }


    dpi /= rho0;
    deltaP[i] = dpi;

    // TODO: move apply here?
}