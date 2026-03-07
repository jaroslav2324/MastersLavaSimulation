// #10
#include "CommonData.hlsl"

// Computes mu_i from log(log(mu+gamma)) = q - y*log(T)
// mu = exp( A * T^{-y} ) - gamma,  A = exp(q)

StructuredBuffer<uint>   particleIndices  : register(t4);
StructuredBuffer<float> temperatureIn   : register(t2); // T_i

RWStructuredBuffer<float> muOut        : register(u12); // μ_i result
RWStructuredBuffer<float> viscCoeffOut : register(u13); // optional normalized coeff 0..1

// TODO: check
[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid];

    float T = temperatureIn[i];
    // 1) safe temperature (avoid zero / negative)
    float Ts = max(T, TminViscosity);

    // 2) compute A = exp(qViscosity)
    // Doing exp(qViscosity) per particle is OK but we can precompute on CPU into a cbuffer if you like.
    float A = exp(qViscosity);

    // 3) compute exponent = A * T^{-yViscosity} = A * exp(-yViscosity * ln T)
    // Use exp/log/pow carefully:
    // pow(Ts, -yViscosity) is equivalent to exp(-yViscosity * log(Ts))
    float powTerm = exp(-yViscosity * log(Ts)); // pow(Ts, -yViscosity)
    float exponent = A * powTerm;

    // 4) clamp exponent to safe range to avoid overflow in exp()
    exponent = clamp(exponent, expClampMinViscosity, expClampMaxViscosity);

    // 5) compute mu = exp(exponent) - gammaViscosity
    float mu = exp(exponent) - gammaViscosity;

    // 6) clamp mu to physical bounds
    mu = clamp(mu, muMinViscosity, muMaxViscosity);

    muOut[i] = mu;

    // 7) optional normalized coefficient for XSPH-style viscosity
    // viscCoeff = clamp(mu / muNormMaxViscosity, 0, 1)
    float coeff = muNormMaxViscosity > 0 ? saturate(mu / muNormMaxViscosity) : 0.0;
    viscCoeffOut[i] = coeff;
}