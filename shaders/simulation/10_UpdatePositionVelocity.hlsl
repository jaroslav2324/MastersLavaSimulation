#include "CommonData.hlsl"

StructuredBuffer<uint>  particleIndices : register(t4);
StructuredBuffer<float> temperatureIn   : register(t2);
StructuredBuffer<uint>  phase           : register(t14);

RWStructuredBuffer<float3> positions  : register(u0);
RWStructuredBuffer<float3> velocities : register(u1);
RWStructuredBuffer<float3> predicted  : register(u7);

static const float collisionvelocityDamping = 0.2f;

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid];

    float3 x_old = positions[i];
    float3 x_new = predicted[i];

    float3 worldMin = worldOrigin;
    float3 worldMax = worldOrigin + float3(gridResolution) * cellSize;

    // Clamp position BEFORE computing velocity: prevents large artificial velocities
    // from deep wall penetration when PBF pushes particles through a boundary.
    float3 x_raw = x_new;
    x_new = clamp(x_new, worldMin, worldMax);

    float3 v = (x_new - x_old) / dt;

    if (x_raw.x < worldMin.x && v.x < 0.0) v.x *= -collisionvelocityDamping;
    else if (x_raw.x > worldMax.x && v.x > 0.0) v.x *= -collisionvelocityDamping;

    if (x_raw.y < worldMin.y && v.y < 0.0) v.y *= -collisionvelocityDamping;
    else if (x_raw.y > worldMax.y && v.y > 0.0) v.y *= -collisionvelocityDamping;

    if (x_raw.z < worldMin.z && v.z < 0.0) v.z *= -collisionvelocityDamping;
    else if (x_raw.z > worldMax.z && v.z > 0.0) v.z *= -collisionvelocityDamping;

    v *= velocityDamping;

    uint curPhase = phase[i];
    if (curPhase == 1)
    {
        float Ti = temperatureIn[i];
        float halfWidth = dampingTransitionWidth * 0.5;
        float tCoef = saturate((Ti - (meltTemperature - halfWidth)) / dampingTransitionWidth);
        v *= lerp(solidVelocityDamping, 1.0f, tCoef);
    }

    velocities[i] = v;
    positions[i]  = x_new;
    predicted[i]  = x_new;
}
