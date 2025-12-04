#include "common_kernels.hlsl"

StructuredBuffer<float3> predicted : register(t0);
StructuredBuffer<uint>   sortedParticleIndices : register(t1);
StructuredBuffer<uint>   cellStart : register(t2);
StructuredBuffer<uint>   cellEnd   : register(t3);

StructuredBuffer<float> lambda : register(t4);

RWStructuredBuffer<float3> deltaP : register(u0);

cbuffer GridParams : register(b1)
{
    int3 gridResolution;  
    float cellSize;       
    float h;              // kernel radius
    float h2;             
    float rho0;           // rest density
};

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;
    float3 pi = predicted[i];
    float3 dpi = float3(0,0,0);

    // λ_i
    float li = lambda[i];

    int3 cell = int3(floor(pi / cellSize));

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
            uint j = sortedParticleIndices[idx];
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
}