#pragma once

#include "pch.h"
#include "StructuredBuffer.h"

struct SimParams
{
    float h;    // kernel radius
    float h2;   // kernel radius squared
    float rho0; // rest density
    float mass; // const, = 1

    float eps;             // small epsilon; const, 1e-6
    float dt;              // delta time
    float epsHeatTransfer; // (0.01 h^2), stabilizer, typically 0.01
    float cellSize;

    float qViscosity; // parameter from viscosity formula
    Vector3 worldOrigin;

    uint32_t numParticles;
    uint32_t gridResolution[3];

    float velocityDamping; // e.g. = 0.99 or 1.0
    Vector3 gravityVec;    // gravity (0, -9.81, 0)

    float yViscosity;           // exponent in viscosity formula
    float gammaViscosity;       // γ offset
    float TminViscosity;        // clamp for temperature (e.g. 1e-3)
    float expClampMinViscosity; // e.g. -80.0

    float expClampMaxViscosity; // e.g.  80.0
    float muMinViscosity;       // minimum mu after clamp
    float muMaxViscosity;       // maximum mu after clamp
    float muNormMaxViscosity;   // value of mu that maps to viscCoeff=1 (for normalization)
};

struct ParticleStateSwapBuffers
{
    // x_i
    winrt::com_ptr<StructuredBuffer> position[2] = {nullptr};

    // q_i*
    winrt::com_ptr<StructuredBuffer> predictedPosition[2] = {nullptr};

    // v_i
    winrt::com_ptr<StructuredBuffer> velocity[2] = {nullptr};

    // T_i
    winrt::com_ptr<StructuredBuffer> temperature[2] = {nullptr};
};

struct ParticleScratchBuffers
{
    //  PBF
    winrt::com_ptr<StructuredBuffer> density = nullptr;     // ρ_i
    winrt::com_ptr<StructuredBuffer> constraintC = nullptr; // C_i
    winrt::com_ptr<StructuredBuffer> lambda = nullptr;      // λ_i
    winrt::com_ptr<StructuredBuffer> deltaP = nullptr;      // Δp_i

    // Viscosity
    winrt::com_ptr<StructuredBuffer> viscosityMu = nullptr;    // μ_i
    winrt::com_ptr<StructuredBuffer> viscosityCoeff = nullptr; // normalized coeff

    // Grid
    winrt::com_ptr<StructuredBuffer> cellStart = nullptr; // per-cell
    winrt::com_ptr<StructuredBuffer> cellEnd = nullptr;

    // Optional
    winrt::com_ptr<StructuredBuffer> phase = nullptr; // solid/liquid/etc
};

struct SortBuffers
{
    // RWStructuredBuffer<uint>
    winrt::com_ptr<StructuredBuffer> hashBuffers[2] = {nullptr};
    // RWStructuredBuffer<uint>
    winrt::com_ptr<StructuredBuffer> indexBuffers[2] = {nullptr};
};