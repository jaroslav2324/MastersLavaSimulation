

cbuffer SimParams : register(b0)
{
    float h;                // kernel radius
    float h2;               // kernel radius squared
    float rho0;             // rest density
    float mass;             // const, = 1

    float eps;              // small epsilon; const, 1e-6
    float dt;               // delta time       
    float epsHeatTransfer;        // (0.01 h^2), stabilizer, typically 0.01
    float  cellSize;

    float qViscosity;         // parameter from viscosity formula
    float3 worldOrigin;

    uint   numParticles;
    uint3  gridResolution; 
    
    float velocityDamping;  // e.g. = 0.99 or 1.0    
    float3 gravityVec; // gravity (0, -9.81, 0)

    float yViscosity;         // exponent in viscosity formula
    float gammaViscosity;     // γ offset
    float TminViscosity;      // clamp for temperature (e.g. 1e-3)
    float expClampMinViscosity; // e.g. -80.0

    float expClampMaxViscosity; // e.g.  80.0
    float muMinViscosity;     // minimum mu after clamp
    float muMaxViscosity;     // maximum mu after clamp
    float muNormMaxViscosity; // value of mu that maps to viscCoeff=1 (for normalization)

};
