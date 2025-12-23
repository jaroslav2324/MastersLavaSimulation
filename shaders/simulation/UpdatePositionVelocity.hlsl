#include "CommonData.hlsl"

RWStructuredBuffer<float3> positions  : register(u0); //x_i (from previous frame) -> x_i (new)
RWStructuredBuffer<float3> predicted  : register(u1); // q_i*
RWStructuredBuffer<float3> velocities : register(u2); // v_i

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;

    float3 x_old = positions[i];
    float3 x_new = predicted[i];

    // Velocity from finite difference
    float3 v = (x_new - x_old) / dt;

    // Optional damping (numerical + physical)
    v *= velocityDamping;

    velocities[i] = v;
    positions[i]  = x_new;

    // TODO: needed?
    predicted[i] = x_new;
}