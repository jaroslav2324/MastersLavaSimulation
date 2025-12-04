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
    // Run one full radix-sort pass over NumElements elements (one full multi-bit run across bits 0..31)
    // This will dispatch compute shaders, perform CPU prefix on block counts and synchronize GPU/CPU.
    // Caller must provide the device's command queue for GPU execution.
    static void SortOnce(ID3D12Device *device, ID3D12CommandQueue *queue, uint32_t NumElements);
    static void SwapParticleBuffers()
    {
        m_currentSwapIndex = (m_currentSwapIndex + 1) % 2;
    }

private:
    // RWStructuredBuffer<Element>
    inline static winrt::com_ptr<ID3D12Resource> sortBuffers[2];
    // uint per block * 2 (zeros/ones)
    inline static winrt::com_ptr<ID3D12Resource> blockCounts;
    // prefix buffer (uint per block * 2)
    inline static winrt::com_ptr<ID3D12Resource> prefixBuffer;
    // readback/upload buffers for CPU-GPU exchange
    inline static winrt::com_ptr<ID3D12Resource> blockCountsReadback;
    inline static winrt::com_ptr<ID3D12Resource> prefixUpload;

    inline static ID3D12DescriptorHeap *m_uavHeap;
    // Compute root signature and PSOs for Count and Scatter kernels
    inline static winrt::com_ptr<ID3D12RootSignature> m_csRootSig;
    inline static winrt::com_ptr<ID3D12PipelineState> m_countPSO;
    inline static winrt::com_ptr<ID3D12PipelineState> m_scatterPSO;

    // Create compute root signature and PSOs for Count and Scatter kernels
    static void CreateSortPipelines(ID3D12Device *device);

    const static int m_maxParticlesCount = 1 << 20; // 1 million particles
    inline static unsigned int m_currentSwapIndex = 0;
    inline static ParticleStateSwapBuffers particleSwapBuffers{};
    inline static ParticleScratchBuffers particleScratchBuffers{};
};