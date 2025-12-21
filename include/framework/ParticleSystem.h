#pragma once

#include <cstdint>
#include <cmath>

#include "pch.h"
#include "Particle.h"
#include <memory>
#include "GPUSorting/GPUSorting.h"
#include "GPUSorting/OneSweep.h"
// simulation kernels
#include "simulation/PredictPositionsKernel.h"
#include "simulation/CellHashKernel.h"
#include "simulation/HashToIndexKernel.h"
#include "simulation/ComputeDensityKernel.h"
#include "simulation/ComputeLambdaKernel.h"
#include "simulation/ComputeDeltaPosKernel.h"
#include "simulation/ApplyDeltaPosKernel.h"
#include "simulation/UpdatePositionVelocityKernel.h"
#include "simulation/ViscosityKernel.h"
#include "simulation/ApplyViscosityKernel.h"
#include "simulation/HeatTransferKernel.h"
#include "simulation/CollisionProjectionKernel.h"

struct SortElement
{
    uint32_t key;
    uint32_t particleIndex;
};

class ParticleSystem
{
public:
    ParticleSystem() = delete;
    static void Init(ID3D12Device *device);

    static void SwapParticleBuffers() // TODO: is this needed?
    {
        m_currentSwapIndex = (m_currentSwapIndex + 1) % 2;
    }

    static void Simulate(float dt);

private:
    static void CreateSimulationRootSignature(ID3D12Device *device);
    static void InitSimulationBuffers(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles, UINT numCells);

    // Simulation parameters stored on CPU and uploaded to GPU CBV
    inline static SimParams m_simParams = {};
    inline static winrt::com_ptr<ID3D12Resource> m_simParamsUpload = nullptr;
    inline static UINT m_simParamsCBV = UINT_MAX;

    // Simulation kernels
    inline static std::unique_ptr<SimulationKernels::PredictPositions> m_predictPositions = nullptr;
    inline static std::unique_ptr<SimulationKernels::CellHash> m_cellHash = nullptr;
    inline static std::unique_ptr<SimulationKernels::HashToIndex> m_hashToIndex = nullptr;
    inline static std::unique_ptr<SimulationKernels::ComputeDensity> m_computeDensity = nullptr;
    inline static std::unique_ptr<SimulationKernels::ComputeLambda> m_computeLambda = nullptr;
    inline static std::unique_ptr<SimulationKernels::ComputeDeltaPos> m_computeDeltaPos = nullptr;
    inline static std::unique_ptr<SimulationKernels::ApplyDeltaPos> m_applyDeltaPos = nullptr;
    inline static std::unique_ptr<SimulationKernels::UpdatePositionVelocity> m_updatePosVel = nullptr;
    inline static std::unique_ptr<SimulationKernels::Viscosity> m_viscosity = nullptr;
    inline static std::unique_ptr<SimulationKernels::ApplyViscosity> m_applyViscosity = nullptr;
    inline static std::unique_ptr<SimulationKernels::HeatTransfer> m_heatTransfer = nullptr;
    inline static std::unique_ptr<OneSweep> m_oneSweep = nullptr;
    inline static std::unique_ptr<SimulationKernels::CollisionProjection> m_collisionProjection = nullptr;

    inline static winrt::com_ptr<ID3D12RootSignature> m_rootSignature = nullptr;

    inline static ID3D12DescriptorHeap *m_uavHeap = nullptr;

    const static int m_gridCellsCount = 256;
    const static int m_maxParticlesCount = 1 << 10;
    inline static unsigned int m_currentSwapIndex = 0;

    inline static ParticleStateSwapBuffers particleSwapBuffers;
    inline static ParticleScratchBuffers particleScratchBuffers;
    inline static SortBuffers sortBuffers;
};