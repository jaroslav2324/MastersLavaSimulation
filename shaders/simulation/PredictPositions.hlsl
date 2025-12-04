cbuffer SimParams : register(b0)
{
    uint  particleCount;
    float dt;
    float3 externalForce; // gravity (0, -9.81, 0)
};

StructuredBuffer<float3> gPositionsSrc     : register(t0); // (x, y, z, radius or padding)
StructuredBuffer<float3> gVelocitySrc      : register(t1); // (vx, vy, vz, pad)

RWStructuredBuffer<float3> gPredictedPositionsDst : register(u0);
RWStructuredBuffer<float3> gVelocityDst           : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x; // TODO: точно правильный индекс?
    if (idx >= particleCount) return;

    float3 pos = gPositionsSrc[idx];
    float3 vel = gVelocitySrc[idx];

    // Apply external forces
    vel.xyz = vel + externalForce * dt;

    // Predict new position
    float3 posPred = pos + vel * dt;

    gVelocityDst[idx] = vel;
    gPredictedPositionsDst[idx] = posPred;
}