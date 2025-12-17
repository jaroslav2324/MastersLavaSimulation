#include "framework/ParticleSystem.h"
#include "framework/RenderTemplatesAPI.h"
#include "framework/ShaderCompiler.h"
#include "framework/RenderSubsystem.h"
#include <stdexcept>
#include <vector>
#include <random>
#include <cstring>

#pragma region INIT
void ParticleSystem::Init(ID3D12Device *device)
{
    // TODO:

    // create constant buffer

    InitSimulationBuffers(device, *RenderSubsystem::GetCBVSRVUAVAllocator(), m_maxParticlesCount, m_gridCellsCount);

    CreateSimulationRootSignature(device);

    // create sort kernels
    // set sort buffers

    // create kernels

    // upload particles
}

void ParticleSystem::CreateSimulationRootSignature(ID3D12Device *device)
{
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        12, // t0 - t11
        0   // base register t0
    );

    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        14, // u0 - u13
        0);

    CD3DX12_ROOT_PARAMETER rootParams[3];

    // SRV table
    rootParams[0].InitAsDescriptorTable(
        1, &srvRange,
        D3D12_SHADER_VISIBILITY_ALL);

    // UAV table
    rootParams[1].InitAsDescriptorTable(
        1, &uavRange,
        D3D12_SHADER_VISIBILITY_ALL);

    // Constant buffer b0
    rootParams[2].InitAsConstantBufferView(
        0, // b0
        0,
        D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(
        _countof(rootParams),
        rootParams,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    winrt::com_ptr<ID3DBlob> serialized;
    winrt::com_ptr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(
        &rsDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serialized.put(),
        error.put()));

    winrt::com_ptr<ID3D12RootSignature> rootSig;
    ThrowIfFailed(device->CreateRootSignature(
        0,
        serialized->GetBufferPointer(),
        serialized->GetBufferSize(),
        IID_PPV_ARGS(rootSig.put())));

    m_rootSignature = rootSig;
}

inline winrt::com_ptr<StructuredBuffer>
CreateBuffer(
    ID3D12Device *device,
    DescriptorAllocator &alloc,
    UINT count,
    UINT stride)
{
    auto buf = winrt::make_self<StructuredBuffer>();
    buf->Init(device, count, stride, alloc);
    return buf;
}

void ParticleSystem::InitSimulationBuffers(
    ID3D12Device *device,
    DescriptorAllocator &alloc,
    UINT numParticles,
    UINT numCells)
{
    for (int i = 0; i < 2; ++i)
    {
        particleSwapBuffers.position[i] = CreateBuffer(
            device, alloc,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.predictedPosition[i] = CreateBuffer(
            device, alloc,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.velocity[i] = CreateBuffer(
            device, alloc,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.temperature[i] = CreateBuffer(
            device, alloc,
            numParticles,
            sizeof(float));
    }

    particleScratchBuffers.density = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(float));

    particleScratchBuffers.constraintC = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(float));

    particleScratchBuffers.lambda = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(float));

    particleScratchBuffers.deltaP = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(DirectX::SimpleMath::Vector3));

    particleScratchBuffers.viscosityMu = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(float));

    particleScratchBuffers.viscosityCoeff = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(float));

    particleScratchBuffers.cellStart = CreateBuffer(
        device, alloc,
        numCells,
        sizeof(uint32_t));

    particleScratchBuffers.cellEnd = CreateBuffer(
        device, alloc,
        numCells,
        sizeof(uint32_t));

    particleScratchBuffers.phase = CreateBuffer(
        device, alloc,
        numParticles,
        sizeof(uint32_t));

    for (int i = 0; i < 2; ++i)
    {
        sortBuffers.hashBuffers[i] = CreateBuffer(
            device, alloc,
            numParticles,
            sizeof(uint32_t));

        sortBuffers.indexBuffers[i] = CreateBuffer(
            device, alloc,
            numParticles,
            sizeof(uint32_t));
    }
}
#pragma endregion

#pragma region SIMULATE
void ParticleSystem::Simulate(float dt)
{
    // update constant buffer

    //     ID3D12DescriptorHeap* heaps[] = {
    //     srvUavAllocator.GetHeap()
    // };
    // cmdList->SetDescriptorHeaps(1, heaps);

    // TODO: in kernel
    //     cmdList->SetComputeRootDescriptorTable(
    //     SRV_TABLE_SLOT,
    //     srvUavAllocator.GetGpuHandle(density.srvIndex)
    // );

    // cmdList->SetComputeRootSignature(m_rootSignature.get());
    // predict position kernel

    // sort

    // cmdList->SetComputeRootSignature(m_rootSignature.get());
    // other kernels
}
#pragma endregion