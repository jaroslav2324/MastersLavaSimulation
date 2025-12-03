//TODO: check

StructuredBuffer<float3> positions : register(t0); // old x
RWStructuredBuffer<float3> predicted : register(u0); // will be updated in-place q += delta
RWStructuredBuffer<float3> deltaP : register(u1);
RWStructuredBuffer<float3> velocities : register(u2); // to update

cbuffer SimParams2 : register(b1)
{
    float dt;
    float damping; // optional
}

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    uint i = gid;
    float3 q = predicted[i];
    float3 dp = deltaP[i];
    float3 qnew = q + dp;
    predicted[i] = qnew;

    // update velocity: v = (qnew - x) / dt
    float3 x = positions[i];
    float3 v = (qnew - x) / dt;
    velocities[i] = v * (1.0 - damping); // optional damping
}