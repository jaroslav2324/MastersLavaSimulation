#include "CommonData.hlsl"

StructuredBuffer<float3> positions : register(t0);    // x_i (old)
RWStructuredBuffer<float3> deltaP : register(t1);     // Δp_i, will be cleared

RWStructuredBuffer<float3> predicted : register(u0);  // q_i*
RWStructuredBuffer<float3> velocities : register(u1); // updated v_i
RWStructuredBuffer<float3> newPositions : register(u2); 

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;

    float3 q  = predicted[i];
    float3 dp = deltaP[i];
    float3 qnew = q + dp;

    float3 xold = positions[i];

    float3 v = (qnew - xold) / dt;
    v *= velocityDamping;

    newPositions[i] = qnew;
    velocities[i] = v;

    deltaP[i] = float3(0,0,0);

    predicted[i] = qnew;
}