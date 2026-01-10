#pragma once

#include "pch.h"
#include "StructuredBuffer.h"

struct SimParams
{
    float h = 0.1f;       // kernel radius
    float h2;             // kernel radius squared
    float rho0 = 2800.0f; // rest density
    float mass = 1.0f;    // const, = 1

    float eps = 1e-3f;     // small epsilon; const, 1e-3
    float dt;              // delta time
    float epsHeatTransfer; // h^2, stabilizer
    float cellSize;

    float qViscosity = 0.1f; // TODO: check; // parameter from viscosity formula
    Vector3 worldOrigin = Vector3(0.0f, 0.0f, 0.0f);

    uint32_t numParticles = 0;
    uint32_t gridResolution[3];

    float velocityDamping = 1.0f; // e.g. = 0.99 or 1.0
    // TODO: Vector3 gravityVec = Vector3(0.0f, -9.81f, 0.0f); // gravity (0, -9.81, 0)
    Vector3 gravityVec = Vector3(0.0f, -0.981f, 0.0f); // gravity (0, -9.81, 0)

    float yViscosity = 1.0f;             // exponent in viscosity formula
    float gammaViscosity = 1.0f;         // γ offset
    float TminViscosity = 1e-3f;         // clamp for temperature (e.g. 1e-3)
    float expClampMinViscosity = -80.0f; // e.g. -80.0

    float expClampMaxViscosity = 80.0f; // e.g.  80.0
    float muMinViscosity = 0.0f;        // minimum mu after clamp
    float muMaxViscosity = 10.0f;       // maximum mu after clamp
    float muNormMaxViscosity = 1.0f;    // value of mu that maps to viscCoeff=1 (for normalization)

    // TODO: init method?
};

// interface for shaders
enum class BufferSrvIndex : UINT
{
    Position = 0,
    Velocity = 1,
    Temperature = 2,
    ParticleHash = 3,
    SortedIndices = 4,
    CellStart = 5,
    CellEnd = 6,
    PredictedPosition = 7,
    Density = 8,
    ConstraintC = 9,
    Lambda = 10,
    DeltaP = 11,
    ViscosityMu = 12,
    ViscosityCoeff = 13,
    NumberOfSrvSlots = 14
};

UINT operator+(UINT offset, BufferSrvIndex index);
UINT operator+(BufferSrvIndex index, UINT offset);

// interface for shaders
enum class BufferUavIndex : UINT
{
    Position = 0,
    Velocity = 1,
    Temperature = 2,
    ParticleHash = 3,
    SortedIndices = 4,
    CellStart = 5,
    CellEnd = 6,
    PredictedPosition = 7,
    Density = 8,
    ConstraintC = 9,
    Lambda = 10,
    DeltaP = 11,
    ViscosityMu = 12,
    ViscosityCoeff = 13,
    NumberOfUavSlots = 14
};

UINT operator+(UINT offset, BufferUavIndex index);
UINT operator+(BufferUavIndex index, UINT offset);

// TODO: to separate header
struct PingPongBuffer
{
    std::shared_ptr<StructuredBuffer> buffers[2] = {nullptr, nullptr};
    UINT readIndex = 0;  // Index for reading (SRV)
    UINT writeIndex = 1; // Index for writing (UAV)

    std::shared_ptr<StructuredBuffer> GetReadBuffer() const
    {
        return buffers[readIndex];
    }

    std::shared_ptr<StructuredBuffer> GetWriteBuffer() const
    {
        return buffers[writeIndex];
    }

    void Swap()
    {
        std::swap(readIndex, writeIndex);
    }
};

struct ParticleStateSwapBuffers
{
    PingPongBuffer position;    // x_i // !!! fake, it is no swap
    PingPongBuffer velocity;    // v_i
    PingPongBuffer temperature; // T_i
};

struct ParticleScratchBuffers
{
    std::shared_ptr<StructuredBuffer> predictedPosition = nullptr; // q_i*

    std::shared_ptr<StructuredBuffer> density = nullptr;     // ρ_i
    std::shared_ptr<StructuredBuffer> constraintC = nullptr; // C_i
    std::shared_ptr<StructuredBuffer> lambda = nullptr;      // λ_i
    std::shared_ptr<StructuredBuffer> deltaP = nullptr;      // Δp_i

    std::shared_ptr<StructuredBuffer> viscosityMu = nullptr;    // μ_i
    std::shared_ptr<StructuredBuffer> viscosityCoeff = nullptr; // normalized coeff

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