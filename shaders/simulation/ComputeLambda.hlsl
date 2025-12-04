#include "common_kernels.hlsl"

StructuredBuffer<float3> predictedPositions : register(t0);
StructuredBuffer<uint> sortedParticleIndecies : register(t1);
StructuredBuffer<uint> cellStart : register(t2);
StructuredBuffer<uint> cellEnd   : register(t3);

StructuredBuffer<float> density : register(t4);
StructuredBuffer<float> constraintC : register(t5);

RWStructuredBuffer<float> lambda : register(u0);

cbuffer GridParams : register(b1)
{
    uint3 gridResolution;
    float   cellSize; 
    float3 worldOrigin;   
    float h2; // kernel support radius squared      
};

// TODO: move to common
uint3 GetCellCoord(float3 p)
{
    return uint3(
        clamp((uint)floor((p.x - worldOrigin.x) / cellSize), 0, gridResolution.x - 1),
        clamp((uint)floor((p.y - worldOrigin.y) / cellSize), 0, gridResolution.y - 1),
        clamp((uint)floor((p.z - worldOrigin.z) / cellSize), 0, gridResolution.z - 1)
    );
}

// TODO: move to common
uint GetCellHash(uint3 c)
{
    return c.x 
         + c.y * gridResolution.x
         + c.z * gridResolution.x * gridResolution.y;
}

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;

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
            uint j = sortedParticleIndecies[idx];
            if (j == i) continue;

            float3 pj = predictedPositions[j];
            float3 rij = pi - pj;

            if (dot(rij, rij) >= h2) continue;

            float3 gradW = cubic_kernel_gradient(rij);

            // TODO: считаем что масса 1, завести константу
            float3 grad_j = - (mass / rho0) * gradW;
            sumGrad2 += dot(grad_j, grad_j);

            grad_i += (mass / rho0) * gradW; 
        }
    }

    // добавляем вклад градиента wrt i
    sumGrad2 += dot(grad_i, grad_i);

    // формула λ // TODO: эпсилон это ху нужно ли добавлять эпсилон в знаменатель?  + eps
    float lam = -Ci / sumGrad2;
    lambda[i] = lam;
}