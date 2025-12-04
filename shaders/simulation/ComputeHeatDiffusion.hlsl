#include "CommonSimulationKernels.hlsl"

//TODO: check...

StructuredBuffer<float3> predictedPositions   : register(t0);
StructuredBuffer<uint>   sortedParticleIndices: register(t1);
StructuredBuffer<uint>   cellStart            : register(t2);
StructuredBuffer<uint>   cellEnd              : register(t3);

StructuredBuffer<float>  temperatureIn        : register(t4); // T_i
StructuredBuffer<float>  kFieldIn             : register(t5); // k_i (optional)
StructuredBuffer<float>  densityIn            : register(t6); // rho_i
StructuredBuffer<float>  massField            : register(t7); // mass per particle, or single mass in cbuffer

RWStructuredBuffer<float> temperatureOut     : register(u0);

cbuffer HeatParams : register(b1)
{
    float dt;
    float h;
    float h2;
    float cp;         // specific heat (assume same cp for now)
    float smallEps;   // e.g. 0.01
    float globalMass; // if you use single mass
    uint  gridNx, gridNy, gridNz;
    float cellSize;
};

static inline float3 cubic_kernel_gradient(float3 r); // from your kernels
static inline float cubic_kernel_height(float3 r);

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;

    float3 pi = predictedPositions[i];
    float Ti = temperatureIn[i];
    float ki = kFieldIn[i];
    float rhoi = densityIn[i];
    float mi = massField[i]; // or globalMass

    // Laplacian accumulator
    float lap = 0.0;

    // get cell coord
    int3 cell = int3(floor(pi / cellSize));

    // neighbor search over 27 cells
    [loop]
    for (int dz=-1; dz<=1; dz++)
    for (int dy=-1; dy<=1; dy++)
    for (int dx=-1; dx<=1; dx++)
    {
        int3 nc = cell + int3(dx,dy,dz);
        if (nc.x < 0 || nc.y < 0 || nc.z < 0 ||
            nc.x >= int(gridNx) || nc.y >= int(gridNy) || nc.z >= int(gridNz)) continue;

        uint hash = nc.x + nc.y * gridNx + nc.z * gridNx * gridNy;
        uint start = cellStart[hash];
        uint end   = cellEnd[hash];
        if (start == 0xFFFFFFFF) continue;

        [loop]
        for (uint idx = start; idx < end; ++idx)
        {
            uint j = sortedParticleIndices[idx];
            if (j == i) continue;

            float3 pj = predictedPositions[j];
            float3 xij = pi - pj;
            float dist2 = dot(xij, xij);
            float h2_local = h2;
            if (dist2 >= h2_local) continue;

            float Tj = temperatureIn[j];
            float kj = kFieldIn[j];
            float mj = massField[j];
            float rhoj = densityIn[j];

            // compute x_ij · ∇W_ij
            float3 gradW = cubic_kernel_gradient(xij); // ∇W wrt i (note sign)
            float num = dot(xij, gradW); // x_ij · ∇W_ij
            float denom = dist2 + 0.01 * h2_local;

            float contrib = 2.0 * (mj / rhoj) * (kj * Tj - ki * Ti) * (num / denom);

            lap += contrib;
        }
    }

    // boundary term: if you have boundary particles, add same form here
    // for now we skip or treat boundaries as special contact particles included in the neighbor lists

    // Now compute time derivative: dT/dt = (1/(rho * cp)) * lap + (Q / (rho*cp))
    // no source term here (Q=0)
    float dTdt = (1.0 / (rhoi * cp)) * lap;

    // explicit Euler
    float Tnew = Ti + dt * dTdt;

    // optional: clamp temperatures to reasonable bounds
    // Tnew = clamp(Tnew, Tmin, Tmax);

    temperatureOut[i] = Tnew;
}