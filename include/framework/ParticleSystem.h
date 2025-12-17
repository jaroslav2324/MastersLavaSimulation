#pragma once

#include <cstdint>
#include <cmath>

#include "pch.h"
#include "Particle.h"

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

    static void SwapParticleBuffers()
    {
        m_currentSwapIndex = (m_currentSwapIndex + 1) % 2;
    }

    static void Simulate(float dt);

private:
    static void CreateSimulationRootSignature(ID3D12Device *device);
    static void InitSimulationBuffers(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles, UINT numCells);

    inline static winrt::com_ptr<ID3D12RootSignature> m_rootSignature = nullptr;

    inline static ID3D12DescriptorHeap *m_uavHeap = nullptr;

    const static int m_gridCellsCount = 256;
    const static int m_maxParticlesCount = 1 << 20;
    inline static unsigned int m_currentSwapIndex = 0;

    inline static ParticleStateSwapBuffers particleSwapBuffers;
    inline static ParticleScratchBuffers particleScratchBuffers;
    inline static SortBuffers sortBuffers;
};