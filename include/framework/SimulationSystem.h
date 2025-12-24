#pragma once

#include "pch.h"
#include "Particle.h"

#include "GPUSorting/GPUSorting.h"
#include "GPUSorting/OneSweep.h"

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

class SimulationSystem
{
public:
    SimulationSystem() = delete;
    static void Init(ID3D12Device *device);

    static void SwapParticleBuffers() // TODO: is this needed?
    {
        m_currentSwapIndex = (m_currentSwapIndex + 1) % 2;
    }

    static void Simulate(float dt);

    static D3D12_GPU_DESCRIPTOR_HANDLE GetPositionBufferSRV();
    static D3D12_GPU_DESCRIPTOR_HANDLE GetTemperatureBufferSRV();
    static uint32_t GetNumParticles() { return m_simParams.numParticles; }
    static float GetKernelRadius() { return m_simParams.h; }

    // Particle initialization helpers
    static std::vector<DirectX::SimpleMath::Vector3> GenerateUniformGridPositions(UINT numParticles);
    static std::vector<DirectX::SimpleMath::Vector3> GenerateDenseBottomWithSphere(UINT numParticles);
    static std::vector<DirectX::SimpleMath::Vector3> GenerateDenseRandomPositions(UINT numParticles, unsigned seed = 1337);
    // Generate temperatures for a set of positions according to scene rules
    static void GenerateTemperaturesForPositions(const std::vector<DirectX::SimpleMath::Vector3> &positions, std::vector<float> &outTemps);

private:
    static void CreateSimulationRootSignature(ID3D12Device *device);
    static void CreateSimulationKernels();
    static void InitSimulationBuffers(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles, UINT numCells);
    static void InitTemperatureBuffer(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles);
    static void InitSortIndexBuffers(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles);

    inline static SimParams m_simParams = {};
    inline static winrt::com_ptr<ID3D12Resource> m_simParamsUpload = nullptr;
    inline static UINT m_simParamsCBV = UINT_MAX;

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
    inline static std::unique_ptr<SimulationKernels::CollisionProjection> m_collisionProjection = nullptr;
    inline static std::unique_ptr<OneSweep> m_oneSweep = nullptr;

    inline static winrt::com_ptr<ID3D12RootSignature> m_rootSignature = nullptr;

    inline static ID3D12DescriptorHeap *m_uavHeap = nullptr;

    // reserved descriptor table bases
    inline static UINT m_srvBase = 0;
    inline static UINT m_uavBase = 0;

    const static int m_gridCellsCount = 1 << 9;
    const static int m_maxParticlesCount = 1 << 13; // TODO: move to params?
    inline static unsigned int m_currentSwapIndex = 0;

    inline static ParticleStateSwapBuffers particleSwapBuffers;
    inline static ParticleScratchBuffers particleScratchBuffers;
    inline static SortBuffers sortBuffers;
};