#include "CommonKernels.hlsl"

StructuredBuffer<float3> predictedPositions     : register(t1);
StructuredBuffer<uint>   particleIndices  : register(t4);
StructuredBuffer<uint>   cellStart              : register(t5);
StructuredBuffer<uint>   cellEnd                : register(t6);
StructuredBuffer<float> density                 : register(t8);   // ρ_i

RWStructuredBuffer<float> temperature        : register(u7);

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid];

    const float maxGrad = 10.0 * h; // to limit extreme heat transfer

    // Read particle data
    float3 pi = predictedPositions[i];
    float Ti  = temperature[i];
    float rhoi = density[i];
    float ki = GetThermalConductivity(Ti);

    float laplacian = 0.0;

    uint3 cell = GetCellCoord(pi);

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        int3 nc = int3(cell) + int3(dx, dy, dz);

// TODO: IsCellValid check
        if (nc.x < 0 || nc.y < 0 || nc.z < 0) continue;
        if (nc.x >= gridResolution.x || nc.y >= gridResolution.y || nc.z >= gridResolution.z) continue;

        uint hash = GetCellHash(uint3(nc));

        uint start = cellStart[hash];
        uint end   = cellEnd[hash];

        [loop]
        for (uint idx = start; idx < end; idx++)
        {
            uint j = particleIndices[idx];
            if (j == i) continue;

            float3 pj = predictedPositions[j];
            float3 rij = pi - pj;
            float r2 = dot(rij, rij);
            if (r2 >= h2) continue;

            float Tj = temperature[j];
            float rhoj = density[j];
            float kj = GetThermalConductivity(Tj);

            // kernel gradient
            float3 gradW = cubic_kernel_gradient(rij);

            // numerator: r_ij · ∇W_ij
            float dotTerm = dot(rij, gradW);
            // clamp to avoid extreme heat transfer
            dotTerm = clamp(dotTerm, -maxGrad, maxGrad); // TODO: remove or leave?

            // denominator: r^2 + 0.01 h^2
            float denom = r2 + epsHeatTransfer;

            // term = (m_j / rho_j) * (k_j*T_j - k_i*T_i) * dotTerm / denom
            float diffTerm = (kj * Tj) - (ki * Ti);

            laplacian += (mass / rhoj) * diffTerm * (dotTerm / denom);
        }
    }

    // Multiply by the leading factor 2 from the formula
    laplacian *= 2.0;

    // Heat equation: dT/dt = k_i * laplacian
    float dT = ki * laplacian;

    // Update temperature
    float newT = Ti + dt * dT;

    temperature[i] = newT;
}