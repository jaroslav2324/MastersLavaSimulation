cbuffer SimParams : register(b0)
{
    float h;
    float h2;
    float rho0;
    float mass;

    float eps;
    float dt;
    float epsHeatTransfer;
    float cellSize;

    float qViscosity;
    float3 worldOrigin;

    uint   numParticles;
    uint3  gridResolution;

    float velocityDamping;
    float3 gravityVec;

    float yViscosity;
    float gammaViscosity;
    float TminViscosity;
    float expClampMinViscosity;

    float expClampMaxViscosity;
    float muMinViscosity;
    float muMaxViscosity;
    float muNormMaxViscosity;

    float freezeTemperature;
    float meltTemperature;
    float freezeDensityFactor;
    float solidVelocityDamping;
    float dampingTransitionWidth;
};
