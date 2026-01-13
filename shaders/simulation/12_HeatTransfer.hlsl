// #12

#include "CommonKernels.hlsl"

StructuredBuffer<float3> predictedPositions : register(t7);
StructuredBuffer<uint>   particleIndices    : register(t4);
StructuredBuffer<uint>   cellStart           : register(t5);
StructuredBuffer<uint>   cellEnd             : register(t6);
StructuredBuffer<float>  density             : register(t8);
StructuredBuffer<float>  temperatureIn       : register(t2);

RWStructuredBuffer<float> temperatureOut     : register(u2);

static const float Tenv = 300.0f;        // воздух TODO: в параметры симуляции
static const float heatLossCoeff = 5.0f; // TODO: в параметры симуляции

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles)
        return;

    uint i = particleIndices[gid];

    float3 pi = predictedPositions[i];
    float  Ti = temperatureIn[i];
    float  rhoi = max(density[i], 1e-6);

    float ki = GetThermalConductivity(Ti);

    float dTdt = 0.0;

    uint3 cell = GetCellCoord(pi);

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        int3 nc = int3(cell) + int3(dx, dy, dz);
        if (any(nc < 0) || any(nc >= int3(gridResolution)))
            continue;

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
            // TODO: move increased kernel radius to params
            if (r2 >= 25.0f * h2)
                continue;

            float Tj = temperatureIn[j];
            float rhoj = max(density[j], 1e-6);
            float kj = GetThermalConductivity(Tj);

            // TODO: move increased kernel radius to params
            float3 gradW = - 5.0f * cubic_kernel_gradient(rij / 5.0f);

            float dotTerm = dot(rij, gradW);
            float denom   = r2 + epsHeatTransfer;

            float kij = (2.0 * ki * kj) / (ki + kj);

            float contrib =
                mass *
                kij *
                (Tj - Ti) *
                dotTerm /
                (rhoi * rhoj * denom);

            // TODO: analyze is ot ok
            dTdt += clamp(contrib, -0.01, 0.01);
        }
    }

    float exposure = saturate((rho0 - rhoi) / rho0);
    exposure = pow(exposure, 1.5); 

    float heatLoss =
        heatLossCoeff *
        exposure *
        (Ti - Tenv);

    dTdt -= heatLoss;

    // TODO: to params?
    float newT = clamp(Ti + dt * dTdt, 0.01f, 2000.0f);

    temperatureOut[i] = newT;
}