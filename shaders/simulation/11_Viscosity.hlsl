// mu = exp( A * T^{-y} ) - gamma,  A = exp(qViscosity)
#include "CommonData.hlsl"

StructuredBuffer<uint>  particleIndices : register(t4);
StructuredBuffer<float> temperatureIn  : register(t2);

RWStructuredBuffer<float> muOut        : register(u12);
RWStructuredBuffer<float> viscCoeffOut : register(u13);

[numthreads(256,1,1)]
void CSMain(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = particleIndices[gid];

    float Ts = max(temperatureIn[i], TminViscosity);

    float A        = exp(qViscosity);
    float exponent = clamp(A * exp(-yViscosity * log(Ts)), expClampMinViscosity, expClampMaxViscosity);
    float mu       = clamp(exp(exponent) - gammaViscosity, muMinViscosity, muMaxViscosity);

    muOut[i]        = mu;
    viscCoeffOut[i] = muNormMaxViscosity > 0 ? saturate(mu / muNormMaxViscosity) : 0.0;
}
