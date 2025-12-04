// TODO: check

StructuredBuffer<float3> predicted : register(t0); // x*
StructuredBuffer<float3> velocities : register(t1);

StructuredBuffer<uint> sortedIndices : register(t2);
StructuredBuffer<uint> cellStart : register(t3);
StructuredBuffer<uint> cellEnd   : register(t4);

RWStructuredBuffer<float3> outVelocities : register(u0);

cbuffer ViscosityParams : register(b1)
{
    float c;   // viscosity coefficient (0.01–0.5)
    float h;   // smoothing radius
}

// TODO: check and use
float ViscosityFromTemp_Arrhenius(float T, float A, float B, float Tref, float muMin, float muMax)
{
    float denom = max(T - Tref, 1e-4);
    float mu = A * exp(B / denom); 
    return clamp(mu, muMin, muMax);
}

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;
    float3 pi = predicted[i];
    float3 vi = velocities[i];
    float3 visc = float3(0,0,0);

    uint cellHash = ComputeHash(pi); // same as in density step

    // TODO: triple for loop
    for (uint nh=0; nh < 27; ++nh)
    {
        uint hash = GetAdjacentHash(cellHash, nh);
        uint start = cellStart[hash];
        uint end   = cellEnd[hash];

        for (uint k = start; k < end; ++k)
        {
            uint j = sortedIndices[k];
            if (j == i) continue;

            float3 pj = predicted[j];
            float3 r = pi - pj;
            float dist2 = dot(r, r);
            if (dist2 >= h*h) continue;

            float3 vj = velocities[j];
            float w = poly6_height(dist2);   // kernel

            visc += (vj - vi) * w;
        }
    }

    float3 vnew = vi + c * visc;
    outVelocities[i] = vnew;
}
