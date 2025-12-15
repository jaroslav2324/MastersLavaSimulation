#include "CommonData.hlsl"

StructuredBuffer<float3> oldPositions : register(t0); // x_i (from previous frame)

RWStructuredBuffer<float3> positions  : register(u12); // x_i (new)
RWStructuredBuffer<float3> predicted  : register(u0); // q_i*
RWStructuredBuffer<float3> velocities : register(u1); // v_i

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;

    float3 x_old = oldPositions[i];
    float3 x_new = predicted[i];

    // Velocity from finite difference
    float3 v = (x_new - x_old) / dt;

    // Optional damping (numerical + physical)
    v *= velocityDamping;

    // Write back
    velocities[i] = v;
    positions[i]  = x_new;

    // Keep predicted in sync (important for next frame)
    predicted[i] = x_new;
}