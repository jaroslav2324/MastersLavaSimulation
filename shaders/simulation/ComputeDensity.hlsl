#include "CommonSimulationKernels.hlsl"

StructuredBuffer<float3> predictedPositions : register(t0);

StructuredBuffer<uint> sortedParticleHashes : register(t1);
StructuredBuffer<uint> sortedParticleIndecies : register(t2);
StructuredBuffer<uint> cellStart : register(t3);
StructuredBuffer<uint> cellEnd : register(t4);

RWStructuredBuffer<float> density : register(u0); // output
RWStructuredBuffer<float> constraintC : register(u1); // optional store C = rho/rho0 - 1

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;
    float3 qi = predictedPositions[i];

    uint cellHash = sortedParticleHashes[i];

    float rho = 0.0;
    uint startNeighbourIndex = cellStart[cellHash];
    uint endNeighbourIndex = cellEnd[cellHash];
    for (uint k=startNeighbourIndex; k < endNeighbourIndex; ++k)
    {
        uint j = sortedParticleIndecies[k];
        float3 qj = predictedPositions[j];
        float3 r = qi - qj;
        rho += mass * cubic_kernel_height(r); // или poly6, смотря что лучше
    }
    // self contribution (optional depending on kernel definition)
    // rho += mass * cubic_kernel_height(float3(0,0,0));

    density[i] = rho;
    constraintC[i] = rho / rho0 - 1.0;
}