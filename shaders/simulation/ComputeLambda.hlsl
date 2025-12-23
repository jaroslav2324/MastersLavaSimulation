#include "CommonKernels.hlsl"

StructuredBuffer<float3> predictedPositions : register(t1);
StructuredBuffer<uint> particleIndecies : register(t4);
StructuredBuffer<uint> cellStart : register(t5);
StructuredBuffer<uint> cellEnd   : register(t6);

StructuredBuffer<float> density : register(t8);
StructuredBuffer<float> constraintC : register(t9);

RWStructuredBuffer<float> lambda : register(u10);

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndecies[gid];

    float3 pi = predictedPositions[i];

    float Ci = constraintC[i]; 

    float sumGrad2 = 0.0;
    float3 grad_i = float3(0,0,0);

    uint3 cell = GetCellCoord(pi);
 
    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        int3 nc = int3(cell) + int3(dx, dy, dz);

        if (nc.x < 0 || nc.y < 0 || nc.z < 0) continue;
        if (nc.x >= gridResolution.x || nc.y >= gridResolution.y || nc.z >= gridResolution.z) continue;

        uint hash = GetCellHash(uint3(nc));

        uint start = cellStart[hash];
        uint end   = cellEnd[hash];

        //if (start == 0xFFFFFFFF) continue;TODO:  remove

        [loop]
        for (uint idx = start; idx < end; idx++)
        {
            uint j = particleIndecies[idx];
            if (j == i) continue;

            float3 pj = predictedPositions[j];
            float3 rij = pi - pj;

            if (dot(rij, rij) >= h2) continue;

            float3 gradW = cubic_kernel_gradient(rij);

            float3 grad_j = - (mass / rho0) * gradW;
            sumGrad2 += dot(grad_j, grad_j);

            grad_i += (mass / rho0) * gradW; 
        }
    }

    // добавляем вклад градиента wrt i
    sumGrad2 += dot(grad_i, grad_i);

    float lam = -Ci / (sumGrad2 + eps);
    lambda[i] = lam;
}