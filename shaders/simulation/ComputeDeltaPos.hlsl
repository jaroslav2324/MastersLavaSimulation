#include "common_kernels.hlsl"

// TODO: check

StructuredBuffer<float3> predicted : register(t0);
StructuredBuffer<int> neighborCounts : register(t1);
StructuredBuffer<int> neighbors : register(t2);

StructuredBuffer<float> lambda : register(t3);

RWStructuredBuffer<float3> deltaP : register(u0);

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;
    float3 qi = predicted[i];
    float3 dpi = float3(0,0,0);

    uint base = i * MAX_NEIGHBORS;
    uint nCount = neighborCounts[i];

    for (uint k=0; k < nCount; ++k)
    {
        int j = neighbors[base + k];
        float3 rij = qi - predicted[j];
        float3 gradW = cubic_kernel_gradient(rij); // grad wrt i (signs consistent)
        float lj = lambda[j];
        float li = lambda[i];
        dpi += (li + lj) * gradW;
    }

    // final scale: dpi /= rho0 (also multiply by weight if using per-particle weights)
    dpi /= rho0;
    deltaP[i] = dpi;
}