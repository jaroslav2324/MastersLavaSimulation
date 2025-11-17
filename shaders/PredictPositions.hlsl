// PredictPositions.hlsl
// PBF Step: apply external forces + predict positions

// ---------------------------------
// CONSTANTS
// ---------------------------------
cbuffer SimParams : register(b0)
{
    uint  particleCount;
    float dt;
    float3 externalForce; // gravity (0, -9.81, 0)
};

// ---------------------------------
// RESOURCES
// ---------------------------------
StructuredBuffer<float4> gPositionsSrc     : register(t0); // (x, y, z, radius or padding)
StructuredBuffer<float4> gVelocitySrc      : register(t1); // (vx, vy, vz, pad)

RWStructuredBuffer<float4> gPredictedPositionsDst : register(u0);
RWStructuredBuffer<float4> gVelocityDst           : register(u1);

// ---------------------------------
// THREADS PER GROUP
// ---------------------------------
[numthreads(256, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;
    if (idx >= particleCount) return;

    float4 pos = gPositionsSrc[idx];
    float4 vel = gVelocitySrc[idx];

    // Apply external forces
    vel.xyz = vel.xyz + externalForce * dt;

    // Predict new position
    float3 posPred = pos.xyz + vel.xyz * dt;

    // Write results
    gVelocityDst[idx]           = float4(vel.xyz, 0.0f);
    gPredictedPositionsDst[idx] = float4(posPred, pos.w);
}