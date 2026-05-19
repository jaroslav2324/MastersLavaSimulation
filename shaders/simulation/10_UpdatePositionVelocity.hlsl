// #9
#include "CommonData.hlsl"

StructuredBuffer<uint>     particleIndices : register(t4);

StructuredBuffer<float>    temperatureIn : register(t2);
StructuredBuffer<uint>     phase         : register(t14);

RWStructuredBuffer<float3> positions  : register(u0);
RWStructuredBuffer<float3> velocities : register(u1);
RWStructuredBuffer<float3> predicted  : register(u7);

static const float collisionvelocityDamping = 0.2f;

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    // TODO: use gid directly as index, without particleIndices indirection?
    uint i = particleIndices[gid];

    float3 x_old = positions[i];
    float3 x_new = predicted[i];

    // Solid particles are static obstacles — keep position fixed, zero velocity
    uint curPhase = phase[i];
    if (curPhase == 1u)
    {
        velocities[i] = float3(0, 0, 0);
        predicted[i]  = x_old;
        return;
    }

    // --- world bounds ---
    float3 worldMin = worldOrigin;
    float3 worldMax = worldOrigin + float3(gridResolution) * cellSize;

    float3 v = (x_new - x_old) / dt;

    // --- X axis ---
    if (x_new.x < worldMin.x)
    {
        x_new.x = worldMin.x;
        if (v.x < 0.0)
            v.x *= -collisionvelocityDamping; 
    }
    else if (x_new.x > worldMax.x)
    {
        x_new.x = worldMax.x;
        if (v.x > 0.0)
            v.x *= -collisionvelocityDamping;
    }

    // --- Y axis ---
    if (x_new.y < worldMin.y)
    {
        x_new.y = worldMin.y;
        if (v.y < 0.0)
            v.y *= -collisionvelocityDamping;
    }
    else if (x_new.y > worldMax.y)
    {
        x_new.y = worldMax.y;
        if (v.y > 0.0)
            v.y *= -collisionvelocityDamping;
    }

    // --- Z axis ---
    if (x_new.z < worldMin.z)
    {
        x_new.z = worldMin.z;
        if (v.z < 0.0)
            v.z *= -collisionvelocityDamping;
    }
    else if (x_new.z > worldMax.z)
    {
        x_new.z = worldMax.z;
        if (v.z > 0.0)
            v.z *= -collisionvelocityDamping;
    }

    // Optional damping
    v *= velocityDamping;

    velocities[i] = v;
    positions[i]  = x_new;
    predicted[i]  = x_new;
}