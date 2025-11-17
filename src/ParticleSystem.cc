#include "ParticleSystem.h"
#include "RenderTemplatesAPI.h"
#include <stdexcept>

void ParticleSystem::Init(ID3D12Device *device)
{
    if (!device)
        throw std::runtime_error("ParticleSystem::Init called with null device");

    const uint64_t count = static_cast<uint64_t>(m_maxParticlesCount);

    // We'll store positions/predictedPositions/velocity as float4 per particle (16 bytes)
    const uint64_t vec4Size = sizeof(float) * 4;
    const uint64_t bufSizeVec4 = vec4Size * count;

    // Temperature and scratch buffers can be single floats per particle.
    const uint64_t floatSize = sizeof(float);
    const uint64_t bufSizeFloat = floatSize * count;

    // Create default heap GPU buffers with UAV flag for unordered access in compute/graphics
    D3D12_HEAP_PROPERTIES heapProps = GetDefaultHeapProperties();

    // Descriptor for vec4 buffers
    D3D12_RESOURCE_DESC descVec4 = GetBufferResourceDesc(bufSizeVec4);
    descVec4.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    // Descriptor for float buffers
    D3D12_RESOURCE_DESC descFloat = GetBufferResourceDesc(bufSizeFloat);
    descFloat.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = S_OK;

    for (int i = 0; i < 2; ++i)
    {
        hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &descVec4,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&particleSwapBuffers.position[i]));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create particle position buffer");

        hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &descVec4,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&particleSwapBuffers.predictedPosition[i]));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create particle predictedPosition buffer");

        hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &descVec4,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&particleSwapBuffers.velocity[i]));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create particle velocity buffer");

        hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &descFloat,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&particleSwapBuffers.temperature[i]));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create particle temperature buffer");
    }

    // Scratch buffers
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &descFloat,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&particleScratchBuffers.density));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create particle density scratch buffer");

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &descFloat,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&particleScratchBuffers.lambda));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create particle lambda scratch buffer");

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &descFloat,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&particleScratchBuffers.deltaP));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create particle deltaP scratch buffer");

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &descFloat,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&particleScratchBuffers.phase));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create particle phase scratch buffer");
}