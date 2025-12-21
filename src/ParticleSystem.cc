#include "framework/ParticleSystem.h"
#include "framework/RenderTemplatesAPI.h"
#include "framework/ShaderCompiler.h"
#include "framework/RenderSubsystem.h"
#include "GPUSorting/OneSweep.h"
#include <stdexcept>
#include <vector>
#include <random>
#include <cstring>
#include <algorithm>
#include <filesystem>

#pragma region INIT
void ParticleSystem::Init(ID3D12Device *device)
{
    // initialize simulation buffers (SRV/UAV descriptors)
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocator();
    InitSimulationBuffers(device, *alloc, m_maxParticlesCount, m_gridCellsCount);

    // create root signature used by simulation kernels
    CreateSimulationRootSignature(device);

    // --- create constant buffer (upload heap) for SimParams ---
    // prepare default sim params
    m_simParams.h = 0.05f;
    m_simParams.h2 = m_simParams.h * m_simParams.h;
    m_simParams.rho0 = 2800.0f;
    m_simParams.mass = 1.0f;
    m_simParams.eps = 1e-6f;
    m_simParams.dt = 1.0f / 60.0f;
    m_simParams.epsHeatTransfer = 0.01f * m_simParams.h * m_simParams.h;
    m_simParams.cellSize = m_simParams.h;
    m_simParams.qViscosity = 0.1f; // TODO: check
    m_simParams.worldOrigin = DirectX::SimpleMath::Vector3(-0.5f, -0.5f, -0.5f);
    m_simParams.numParticles = 0; // will set after particle generation
    // grid resolution: cubic approximation
    int gridRes = std::max(1, (int)std::round(std::cbrt((double)m_gridCellsCount)));
    m_simParams.gridResolution[0] = gridRes;
    m_simParams.gridResolution[1] = gridRes;
    m_simParams.gridResolution[2] = gridRes;
    m_simParams.velocityDamping = 1.0f;
    m_simParams.gravityVec = DirectX::SimpleMath::Vector3(0.0f, -9.81f, 0.0f);
    m_simParams.yViscosity = 1.0f;
    m_simParams.gammaViscosity = 1.0f;
    m_simParams.TminViscosity = 1e-3f;
    m_simParams.expClampMinViscosity = -80.0f;
    m_simParams.expClampMaxViscosity = 80.0f;
    m_simParams.muMinViscosity = 0.0f;
    m_simParams.muMaxViscosity = 10.0f;
    m_simParams.muNormMaxViscosity = 1.0f;

    // create upload resource for constant buffer (256-byte aligned)
    const UINT64 cbSizeUnaligned = sizeof(SimParams);
    const UINT64 cbSize = (cbSizeUnaligned + 255) & ~255ULL;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_simParamsUpload.put())));

    // map and copy
    void *pData = nullptr;
    D3D12_RANGE readRange{0, 0};
    ThrowIfFailed(m_simParamsUpload->Map(0, &readRange, &pData));
    memcpy(pData, &m_simParams, sizeof(SimParams));
    m_simParamsUpload->Unmap(0, nullptr);

    // create CBV descriptor
    m_simParamsCBV = alloc->Alloc();
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = m_simParamsUpload->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = static_cast<UINT>(cbSize);
    device->CreateConstantBufferView(&cbvDesc, alloc->GetCpuHandle(m_simParamsCBV));

    // create simulation kernels
    GPUSorting::DeviceInfo devInfo = RenderSubsystem::GetDeviceInfo();
    std::vector<std::wstring> compileArgs;
    auto devicePtr = RenderSubsystem::GetDevice();
    std::filesystem::path shaderBase = L"shaders/simulation";

    m_predictPositions = std::make_unique<SimulationKernels::PredictPositions>(
        devicePtr, devInfo, compileArgs, shaderBase / L"PredictPositions.hlsl", m_rootSignature);

    m_cellHash = std::make_unique<SimulationKernels::CellHash>(
        devicePtr, devInfo, compileArgs, shaderBase / L"CellHash.hlsl", m_rootSignature);

    m_hashToIndex = std::make_unique<SimulationKernels::HashToIndex>(
        devicePtr, devInfo, compileArgs, shaderBase / L"HashToIndex.hlsl", m_rootSignature);

    m_computeDensity = std::make_unique<SimulationKernels::ComputeDensity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"ComputeDensity.hlsl", m_rootSignature);

    m_computeLambda = std::make_unique<SimulationKernels::ComputeLambda>(
        devicePtr, devInfo, compileArgs, shaderBase / L"ComputeLambda.hlsl", m_rootSignature);

    m_computeDeltaPos = std::make_unique<SimulationKernels::ComputeDeltaPos>(
        devicePtr, devInfo, compileArgs, shaderBase / L"ComputeDeltaPos.hlsl", m_rootSignature);

    m_applyDeltaPos = std::make_unique<SimulationKernels::ApplyDeltaPos>(
        devicePtr, devInfo, compileArgs, shaderBase / L"ApplyDeltaPos.hlsl", m_rootSignature);

    m_updatePosVel = std::make_unique<SimulationKernels::UpdatePositionVelocity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"UpdatePositionVelocity.hlsl", m_rootSignature);

    m_viscosity = std::make_unique<SimulationKernels::Viscosity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"Viscosity.hlsl", m_rootSignature);

    m_applyViscosity = std::make_unique<SimulationKernels::ApplyViscosity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"ApplyViscosity.hlsl", m_rootSignature);

    m_heatTransfer = std::make_unique<SimulationKernels::HeatTransfer>(
        devicePtr, devInfo, compileArgs, shaderBase / L"HeatTransfer.hlsl", m_rootSignature);

    m_collisionProjection = std::make_unique<SimulationKernels::CollisionProjection>(
        devicePtr, devInfo, compileArgs, shaderBase / L"CollisionProjection.hlsl", m_rootSignature);

    // instantiate OneSweep sorter (keys = hash, payload = particle index)
    m_oneSweep = std::make_unique<OneSweep>(devicePtr, devInfo, GPUSorting::ORDER_ASCENDING, GPUSorting::KEY_UINT32, GPUSorting::PAYLOAD_UINT32);

    m_simParams.numParticles = m_maxParticlesCount;

    // grid dimensions
    uint32_t dim = static_cast<uint32_t>(std::ceil(std::pow((double)m_maxParticlesCount, 1.0 / 3.0)));
    std::vector<DirectX::SimpleMath::Vector3> hostPositions;
    hostPositions.reserve(m_maxParticlesCount);
    for (uint32_t z = 0; z < dim && hostPositions.size() < m_maxParticlesCount; ++z)
    {
        for (uint32_t y = 0; y < dim && hostPositions.size() < m_maxParticlesCount; ++y)
        {
            for (uint32_t x = 0; x < dim && hostPositions.size() < m_maxParticlesCount; ++x)
            {
                float fx = (x + 0.5f) / (float)dim - 0.5f;
                float fy = (y + 0.5f) / (float)dim - 0.5f;
                float fz = (z + 0.5f) / (float)dim - 0.5f;
                hostPositions.emplace_back(fx, fy, fz);
            }
        }
    }

    // create upload buffer
    UINT64 uploadSize = UINT64(m_maxParticlesCount) * sizeof(DirectX::SimpleMath::Vector3);
    CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    winrt::com_ptr<ID3D12Resource> uploadResource;
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadResource.put())));

    // copy data to upload
    void *pUpload = nullptr;
    ThrowIfFailed(uploadResource->Map(0, &readRange, &pUpload));
    memcpy(pUpload, hostPositions.data(), (size_t)uploadSize);
    uploadResource->Unmap(0, nullptr);

    // prepare temp command allocator/list to perform GPU copy
    winrt::com_ptr<ID3D12CommandAllocator> cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.put())));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.get(), nullptr, IID_PPV_ARGS(cmdList.put())));

    // transition destination to COPY_DEST
    auto dstRes = particleSwapBuffers.position[0]->resource.get();
    CD3DX12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(dstRes, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &toCopy);

    // copy
    cmdList->CopyBufferRegion(dstRes, 0, uploadResource.get(), 0, uploadSize);

    // transition to UAV for compute use
    CD3DX12_RESOURCE_BARRIER toUAV = CD3DX12_RESOURCE_BARRIER::Transition(dstRes, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->ResourceBarrier(1, &toUAV);

    ThrowIfFailed(cmdList->Close());

    ID3D12CommandList *lists[] = {cmdList.get()};
    auto queue = RenderSubsystem::GetCommandQueue();
    queue->ExecuteCommandLists(1, lists);

    // wait for completion
    winrt::com_ptr<ID3D12Fence> fence;
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
    uint64_t fenceVal = 1;
    queue->Signal(fence.get(), fenceVal);
    if (fence->GetCompletedValue() < fenceVal)
    {
        HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(fenceVal, ev);
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
    }

    // free upload resource (will release on function exit)

    // TODO: what for?
    // duplicate positions into second swap buffer so both contain valid data
    // (simple copy on CPU to GPU would be similar; here we re-use the same upload resource)
    // For brevity we copy the same data into the second buffer via another command list
    // create second command list
    ThrowIfFailed(cmdAlloc->Reset());
    ThrowIfFailed(cmdList->Reset(cmdAlloc.get(), nullptr));
    auto dstRes1 = particleSwapBuffers.position[1]->resource.get();
    CD3DX12_RESOURCE_BARRIER toCopy1 = CD3DX12_RESOURCE_BARRIER::Transition(dstRes1, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &toCopy1);
    cmdList->CopyBufferRegion(dstRes1, 0, uploadResource.get(), 0, uploadSize);
    CD3DX12_RESOURCE_BARRIER toUAV1 = CD3DX12_RESOURCE_BARRIER::Transition(dstRes1, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->ResourceBarrier(1, &toUAV1);
    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists2[] = {cmdList.get()};
    queue->ExecuteCommandLists(1, lists2);
    fenceVal++;
    queue->Signal(fence.get(), fenceVal);
    if (fence->GetCompletedValue() < fenceVal)
    {
        HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(fenceVal, ev);
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
    }

    // TODO: fill index sort buffer?

    // update CPU-side numParticles in CB (mapped copy)
    ThrowIfFailed(m_simParamsUpload->Map(0, &readRange, &pData));
    memcpy(pData, &m_simParams, sizeof(SimParams));
    m_simParamsUpload->Unmap(0, nullptr);
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
    // update time step
    m_simParams.dt = dt;

    // copy updated SimParams into the upload constant buffer
    if (m_simParamsUpload)
    {
        D3D12_RANGE readRange{0, 0};
        void *pData = nullptr;
        ThrowIfFailed(m_simParamsUpload->Map(0, &readRange, &pData));
        memcpy(pData, &m_simParams, sizeof(SimParams));
        m_simParamsUpload->Unmap(0, nullptr);
    }

    // prepare command allocator and list
    auto device = RenderSubsystem::GetDevice();
    winrt::com_ptr<ID3D12CommandAllocator> cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.put())));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.get(), nullptr, IID_PPV_ARGS(cmdList.put())));

    // set descriptor heaps (CBV/SRV/UAV heap used by StructuredBuffer descriptors)
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocator();
    ID3D12DescriptorHeap *heaps[] = {alloc->GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);

    // bind SimParams CBV at root 0 for kernels that expect it
    cmdList->SetComputeRootConstantBufferView(0, m_simParamsUpload->GetGPUVirtualAddress());

    const uint32_t numParticles = static_cast<uint32_t>(m_simParams.numParticles);
    // determine source/destination swap indices
    const uint32_t src = m_currentSwapIndex;
    const uint32_t dst = (m_currentSwapIndex + 1) % 2;

    // GPU addresses
    auto positionsSrc = particleSwapBuffers.position[src]->resource->GetGPUVirtualAddress();
    auto velocitySrc = particleSwapBuffers.velocity[src]->resource->GetGPUVirtualAddress();
    auto predictedDst = particleSwapBuffers.predictedPosition[dst]->resource->GetGPUVirtualAddress();
    auto velocityDst = particleSwapBuffers.velocity[dst]->resource->GetGPUVirtualAddress();

    // 1) Predict positions
    m_predictPositions->Dispatch(cmdList, numParticles, positionsSrc, velocitySrc, predictedDst, velocityDst);

    // 2) Simple collision projection (in-place on predicted/velocity buffers)
    m_collisionProjection->Dispatch(cmdList, numParticles, predictedDst, velocityDst);

    // 3) Compute spatial hash into sort buffers (use first pair)
    m_cellHash->Dispatch(cmdList, numParticles,
                         predictedDst,
                         sortBuffers.hashBuffers[0]->resource->GetGPUVirtualAddress(),
                         sortBuffers.indexBuffers[0]->resource->GetGPUVirtualAddress());

    // close and execute command list to ensure hashing is finished before sorting
    // TODO: or wait somehow

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists[] = {cmdList.get()};
    auto queue = RenderSubsystem::GetCommandQueue();
    queue->ExecuteCommandLists(1, lists);

    // wait for completion
    winrt::com_ptr<ID3D12Fence> fence;
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
    uint64_t fenceVal = 1;
    queue->Signal(fence.get(), fenceVal);
    if (fence->GetCompletedValue() < fenceVal)
    {
        HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(fenceVal, ev);
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
    }

    // 4) Configure OneSweep to use our hash/index buffers and sort
    if (m_oneSweep)
    {
        m_oneSweep->SetAllBuffers(
            sortBuffers.hashBuffers[0]->resource,
            sortBuffers.indexBuffers[0]->resource,
            sortBuffers.hashBuffers[1]->resource,
            sortBuffers.indexBuffers[1]->resource,
            true);
        m_oneSweep->Sort();
    }

    // After sorting, hashes and indices are in sortBuffers.hashBuffers[0] and sortBuffers.indexBuffers[0]

    // TODO: probaply one list enough?
    ThrowIfFailed(cmdAlloc->Reset());
    ThrowIfFailed(cmdList->Reset(cmdAlloc.get(), nullptr));
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetComputeRootConstantBufferView(0, m_simParamsUpload->GetGPUVirtualAddress());

    // 5) (hash->cell start)
    m_hashToIndex->Dispatch(cmdList, numParticles,
                            sortBuffers.hashBuffers[0]->resource->GetGPUVirtualAddress(),
                            particleScratchBuffers.cellStart->resource->GetGPUVirtualAddress(),
                            particleScratchBuffers.cellEnd->resource->GetGPUVirtualAddress());

    // 6) PBF solver iterations (3 iterations)
    for (int iter = 0; iter < 3; ++iter)
    {
        // Compute density
        m_computeDensity->Dispatch(cmdList, numParticles,
                                   predictedDst,
                                   sortBuffers.indexBuffers[0]->resource->GetGPUVirtualAddress(),
                                   particleScratchBuffers.cellStart->resource->GetGPUVirtualAddress(),
                                   particleScratchBuffers.cellEnd->resource->GetGPUVirtualAddress(),
                                   particleScratchBuffers.density->resource->GetGPUVirtualAddress(),
                                   particleScratchBuffers.constraintC->resource->GetGPUVirtualAddress());

        // Compute lambda (constraint multipliers)
        m_computeLambda->Dispatch(cmdList, numParticles,
                                  predictedDst,
                                  sortBuffers.indexBuffers[0]->resource->GetGPUVirtualAddress(),
                                  particleScratchBuffers.cellStart->resource->GetGPUVirtualAddress(),
                                  particleScratchBuffers.cellEnd->resource->GetGPUVirtualAddress(),
                                  particleScratchBuffers.density->resource->GetGPUVirtualAddress(),
                                  particleScratchBuffers.constraintC->resource->GetGPUVirtualAddress(),
                                  particleScratchBuffers.lambda->resource->GetGPUVirtualAddress());

        // Compute position corrections
        m_computeDeltaPos->Dispatch(cmdList, numParticles,
                                    predictedDst,
                                    sortBuffers.indexBuffers[0]->resource->GetGPUVirtualAddress(),
                                    particleScratchBuffers.cellStart->resource->GetGPUVirtualAddress(),
                                    particleScratchBuffers.cellEnd->resource->GetGPUVirtualAddress(),
                                    particleScratchBuffers.lambda->resource->GetGPUVirtualAddress(),
                                    particleScratchBuffers.deltaP->resource->GetGPUVirtualAddress());

        // Apply corrections (in-place on predicted positions)
        m_applyDeltaPos->Dispatch(cmdList, numParticles,
                                  predictedDst,
                                  particleScratchBuffers.deltaP->resource->GetGPUVirtualAddress());
    }

    // 7) Update positions and velocities (write to position and velocity dst buffers)
    m_updatePosVel->Dispatch(cmdList, numParticles,
                             positionsSrc,
                             particleSwapBuffers.position[dst]->resource->GetGPUVirtualAddress(),
                             predictedDst,
                             particleSwapBuffers.velocity[dst]->resource->GetGPUVirtualAddress());

    // 8) Viscosity: compute viscosity mu and coefficient from temperature
    m_viscosity->Dispatch(cmdList, numParticles,
                          particleSwapBuffers.temperature[src]->resource->GetGPUVirtualAddress(),
                          particleScratchBuffers.viscosityMu->resource->GetGPUVirtualAddress(),
                          particleScratchBuffers.viscosityCoeff->resource->GetGPUVirtualAddress());

    // 9) Apply viscosity to velocities
    m_applyViscosity->Dispatch(cmdList, numParticles,
                               predictedDst,
                               particleSwapBuffers.velocity[dst]->resource->GetGPUVirtualAddress(),
                               sortBuffers.indexBuffers[0]->resource->GetGPUVirtualAddress(),
                               particleScratchBuffers.cellStart->resource->GetGPUVirtualAddress(),
                               particleScratchBuffers.cellEnd->resource->GetGPUVirtualAddress(),
                               particleScratchBuffers.viscosityCoeff->resource->GetGPUVirtualAddress(),
                               particleSwapBuffers.velocity[dst]->resource->GetGPUVirtualAddress());

    // 10) Heat transfer (temperature diffusion)
    m_heatTransfer->Dispatch(cmdList, numParticles,
                             predictedDst,
                             sortBuffers.indexBuffers[0]->resource->GetGPUVirtualAddress(),
                             particleScratchBuffers.cellStart->resource->GetGPUVirtualAddress(),
                             particleScratchBuffers.cellEnd->resource->GetGPUVirtualAddress(),
                             particleSwapBuffers.temperature[src]->resource->GetGPUVirtualAddress(),
                             particleScratchBuffers.density->resource->GetGPUVirtualAddress(),
                             particleSwapBuffers.temperature[dst]->resource->GetGPUVirtualAddress());

    // finalize remaining GPU work
    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists2[] = {cmdList.get()};
    queue->ExecuteCommandLists(1, lists2);

    // wait for final completion
    fenceVal++;
    queue->Signal(fence.get(), fenceVal);
    if (fence->GetCompletedValue() < fenceVal)
    {
        HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(fenceVal, ev);
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
    }
}
#pragma endregion