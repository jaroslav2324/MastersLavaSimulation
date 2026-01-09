#include "CommonKernels.hlsl"

StructuredBuffer<float3> predicted        : register(t1);
StructuredBuffer<uint>   particleIndices  : register(t4);
StructuredBuffer<uint>   cellStart        : register(t5);
StructuredBuffer<uint>   cellEnd          : register(t6);

StructuredBuffer<float> lambda            : register(t10); // read lambda

RWStructuredBuffer<float3> deltaP          : register(u11); // write deltaP

// ---- tunable parameters ----
static const float kTensile = 0.001f;   // 0.001 .. 0.01
static const float nTensile = 4.0f;
static const float deltaQ  = 0.2f;      // in units of h

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;

    uint i = particleIndices[gid];
    float3 pi = predicted[i];
    float3 dpi = float3(0,0,0);

    float li = lambda[i];

    int3 cell = GetCellCoord(pi);

    // Precompute kernel value at deltaQ
    float Wdq = cubic_kernel_height(float3(deltaQ * h, 0, 0));

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
            uint j = particleIndices[idx];
            if (j == i) continue;

            float3 pj = predicted[j];
            float3 rij = pi - pj;

            float dist2 = dot(rij, rij);
            if (dist2 >= h2) continue;

            float3 gradW = cubic_kernel_gradient(rij);
            float lj = lambda[j];

            // --- tensile instability correction ---
            float W = cubic_kernel_height(rij);
            float scorr = -kTensile * pow(W / Wdq, nTensile);

            dpi += (li + lj + scorr) * gradW;
        }
    }

    float maxDelta = 0.5f * h;

    float len = length(dpi);
    if (len > maxDelta)
        dpi *= maxDelta / len;

    deltaP[i] = dpi / rho0;
}