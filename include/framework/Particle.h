#pragma once

#include "pch.h"
#include "StructuredBuffer.h"

struct SimParams
{
    float h = 0.05f;      // kernel radius
    float h2;             // kernel radius squared
    float rho0 = 2800.0f; // rest density
    float mass = 1.0f;    // const, = 1

    float eps = 1e-6f;     // small epsilon; const, 1e-6
    float dt;              // delta time
    float epsHeatTransfer; // (0.01 h^2), stabilizer, typically 0.01
    float cellSize;

    float qViscosity = 0.1f; // TODO: check; // parameter from viscosity formula
    Vector3 worldOrigin = Vector3(-0.5f, -0.5f, -0.5f);

    uint32_t numParticles = 0;
    uint32_t gridResolution[3];

    float velocityDamping = 1.0f;                     // e.g. = 0.99 or 1.0
    Vector3 gravityVec = Vector3(0.0f, -9.81f, 0.0f); // gravity (0, -9.81, 0)

    float yViscosity = 1.0f;             // exponent in viscosity formula
    float gammaViscosity = 1.0f;         // γ offset
    float TminViscosity = 1e-3f;         // clamp for temperature (e.g. 1e-3)
    float expClampMinViscosity = -80.0f; // e.g. -80.0

    float expClampMaxViscosity = 80.0f; // e.g.  80.0
    float muMinViscosity = 0.0f;        // minimum mu after clamp
    float muMaxViscosity = 10.0f;       // maximum mu after clamp
    float muNormMaxViscosity = 1.0f;    // value of mu that maps to viscCoeff=1 (for normalization)

    // TODO: init method
};

struct ParticleStateSwapBuffers
{
    // x_i
    std::shared_ptr<StructuredBuffer> position[2] = {nullptr};

    // q_i*
    std::shared_ptr<StructuredBuffer> predictedPosition[2] = {nullptr};

    // v_i
    std::shared_ptr<StructuredBuffer> velocity[2] = {nullptr};

    // T_i
    std::shared_ptr<StructuredBuffer> temperature[2] = {nullptr};
};

struct ParticleScratchBuffers
{
    //  PBF
    std::shared_ptr<StructuredBuffer> density = nullptr;     // ρ_i
    std::shared_ptr<StructuredBuffer> constraintC = nullptr; // C_i
    std::shared_ptr<StructuredBuffer> lambda = nullptr;      // λ_i
    std::shared_ptr<StructuredBuffer> deltaP = nullptr;      // Δp_i

    // Viscosity
    std::shared_ptr<StructuredBuffer> viscosityMu = nullptr;    // μ_i
    std::shared_ptr<StructuredBuffer> viscosityCoeff = nullptr; // normalized coeff

    // Grid
    std::shared_ptr<StructuredBuffer> cellStart = nullptr; // per-cell
    std::shared_ptr<StructuredBuffer> cellEnd = nullptr;

    // Optional
    std::shared_ptr<StructuredBuffer> phase = nullptr; // solid/liquid/etc
};

struct SortBuffers
{
    // RWStructuredBuffer<uint>
    std::shared_ptr<StructuredBuffer> hashBuffers[2] = {nullptr};
    // RWStructuredBuffer<uint>
    std::shared_ptr<StructuredBuffer> indexBuffers[2] = {nullptr};
};