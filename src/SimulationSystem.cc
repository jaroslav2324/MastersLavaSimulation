#include "pch.h"

#include "framework/SimulationSystem.h"
#include "framework/RenderTemplatesAPI.h"
#include "framework/ShaderCompiler.h"
#include "framework/RenderSubsystem.h"
#include "framework/UploadHelpers.h"
#include "GPUSorting/OneSweep.h"
#include <random>

// Particle generation helpers
std::vector<DirectX::SimpleMath::Vector3> SimulationSystem::GenerateUniformGridPositions(UINT numParticles)
{
    std::vector<DirectX::SimpleMath::Vector3> out;
    out.reserve(numParticles);
    int n = static_cast<int>(std::ceil(std::cbrt((double)numParticles)));
    for (int z = 0; z < n && out.size() < numParticles; ++z)
    {
        for (int y = 0; y < n && out.size() < numParticles; ++y)
        {
            for (int x = 0; x < n && out.size() < numParticles; ++x)
            {
                float fx = (x + 0.5f) / (float)n;
                float fy = (y + 0.5f) / (float)n;
                float fz = (z + 0.5f) / (float)n;
                out.emplace_back(fx, fy, fz);
            }
        }
    }
    return out;
}

std::vector<DirectX::SimpleMath::Vector3> SimulationSystem::GenerateDenseRandomPositions(UINT numParticles, unsigned seed)
{
    std::vector<DirectX::SimpleMath::Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    // Stratified jittering on a grid for even coverage but denser overall
    int m = static_cast<int>(std::ceil(std::cbrt((double)numParticles)));
    for (int z = 0; z < m && out.size() < numParticles; ++z)
    {
        for (int y = 0; y < m && out.size() < numParticles; ++y)
        {
            for (int x = 0; x < m && out.size() < numParticles; ++x)
            {
                float ox = (x + u(rng)) / (float)m;
                float oy = (y + u(rng)) / (float)m;
                float oz = (z + u(rng)) / (float)m;
                out.emplace_back(ox, oy, oz);
            }
        }
    }
    return out;
}

std::vector<DirectX::SimpleMath::Vector3> SimulationSystem::GenerateDenseBottomWithSphere(UINT numParticles)
{
    std::vector<DirectX::SimpleMath::Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> u(0.0f, 5.0f);

    // Allocate fraction of particles to sphere at top-center
    const UINT sphereCount = static_cast<UINT>(std::round(numParticles * 0.18f));
    const UINT bottomCount = numParticles - sphereCount;

    for (UINT i = 0; i < bottomCount; ++i)
    {
        float x = u(rng);
        float z = u(rng);
        float y = u(rng) / 5.0f;
        out.emplace_back(x, y, z);
    }

    DirectX::SimpleMath::Vector3 center(2.5f, 2.82f, 2.5f);
    const float radius = 1.0f;
    std::uniform_real_distribution<float> uSphere(-radius, radius);
    while (out.size() < numParticles)
    {
        float rx = uSphere(rng);
        float ry = uSphere(rng);
        float rz = uSphere(rng);
        if (rx * rx + ry * ry + rz * rz <= radius * radius)
        {
            DirectX::SimpleMath::Vector3 p = center + DirectX::SimpleMath::Vector3(rx, ry, rz);
            out.push_back(p);
        }
    }

    return out;
}

void SimulationSystem::GenerateTemperaturesForPositions(
    const std::vector<DirectX::SimpleMath::Vector3> &positions,
    std::vector<float> &outTemps)
{
    const float hotHeight = 1.8f; // TODO: can be moved to args

    outTemps.clear();
    outTemps.reserve(positions.size());

    std::mt19937 rngTemp(424242);

    std::uniform_real_distribution<float> coldRange(700.0f, 900.0f);
    std::uniform_real_distribution<float> hotRange(1200.0f, 1400.0f);

    for (const auto &p : positions)
    {
        if (p.y >= hotHeight)
            outTemps.push_back(hotRange(rngTemp));
        else
            outTemps.push_back(coldRange(rngTemp));
    }
}

#pragma region INIT
void SimulationSystem::Init(ID3D12Device *device)
{
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocatorGPUVisible();
    InitSimulationBuffers(device, *alloc, m_maxParticlesCount, m_gridCellsCount);

    CreateSimulationRootSignature(device);

    // fill some default parameters
    m_simParams.h2 = m_simParams.h * m_simParams.h;
    m_simParams.dt = 1.0f / 60.0f;
    m_simParams.epsHeatTransfer = m_simParams.h2;
    m_simParams.cellSize = m_simParams.h * 4.0f; // TODO: be careful with this
    // grid resolution: cubic approximation
    int gridRes = std::max(1, (int)std::round(std::cbrt((double)m_gridCellsCount)));
    m_simParams.gridResolution[0] = gridRes;
    m_simParams.gridResolution[1] = gridRes;
    m_simParams.gridResolution[2] = gridRes;

    const UINT64 cbSizeUnaligned = sizeof(SimParams);
    const UINT64 cbSize = Align256(cbSizeUnaligned);

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

    CreateSimulationKernels();

    std::vector<DirectX::SimpleMath::Vector3> hostPositions = GenerateDenseBottomWithSphere(m_maxParticlesCount);

    // create upload buffer and copy positions into GPU position buffers using one command list
    UINT64 uploadSize = UINT64(m_maxParticlesCount) * sizeof(DirectX::SimpleMath::Vector3);
    auto uploadResource = UploadHelpers::CreateUploadBuffer(device, uploadSize);

    // copy data to upload resource (positions)
    void *pUpload = nullptr;
    ThrowIfFailed(uploadResource->Map(0, &readRange, &pUpload));
    memcpy(pUpload, hostPositions.data(), (size_t)uploadSize);
    uploadResource->Unmap(0, nullptr);

    // generate temperatures for the positions (centralized helper)
    std::vector<float> hostTemps;
    GenerateTemperaturesForPositions(hostPositions, hostTemps);

    // upload temperature buffer
    UINT64 tempUploadSize = UINT64(m_maxParticlesCount) * sizeof(float);
    auto uploadTempResource = UploadHelpers::CreateUploadBuffer(device, tempUploadSize);
    void *pUploadTemp = nullptr;
    ThrowIfFailed(uploadTempResource->Map(0, &readRange, &pUploadTemp));
    memcpy(pUploadTemp, hostTemps.data(), (size_t)tempUploadSize);
    uploadTempResource->Unmap(0, nullptr);

    // prepare single command allocator/list to perform GPU copies for both swap buffers
    winrt::com_ptr<ID3D12CommandAllocator> cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.put())));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.get(), nullptr, IID_PPV_ARGS(cmdList.put())));

    // copy into first swap buffer (COMMON -> COPY -> UAV)
    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadResource.get(),
        particleSwapBuffers.position.buffers[0]->resource.get(),
        uploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // copy into second swap buffer (COMMON -> COPY -> UAV)
    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadResource.get(),
        particleSwapBuffers.position.buffers[1]->resource.get(),
        uploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // copy temperature upload into both temperature swap buffers
    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadTempResource.get(),
        particleSwapBuffers.temperature.buffers[0]->resource.get(),
        tempUploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadTempResource.get(),
        particleSwapBuffers.temperature.buffers[1]->resource.get(),
        tempUploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists[] = {cmdList.get()};
    auto queue = RenderSubsystem::GetCommandQueue();
    queue->ExecuteCommandLists(1, lists);

    // create a fence and wait once for the uploads to complete
    winrt::com_ptr<ID3D12Fence> fence;
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
    uint64_t fenceVal = 1;
    RenderSubsystem::WaitForFence(fence.get(), fenceVal);

    InitSortIndexBuffers(device, *alloc, m_maxParticlesCount);

    m_simParams.numParticles = m_maxParticlesCount;
    ThrowIfFailed(m_simParamsUpload->Map(0, &readRange, &pData));
    memcpy(pData, &m_simParams, sizeof(SimParams));
    m_simParamsUpload->Unmap(0, nullptr);
}

void SimulationSystem::CreateSimulationRootSignature(ID3D12Device *device)
{
    CD3DX12_DESCRIPTOR_RANGE ranges[8];

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // Position SRV
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // Position UAV

    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // Velocity SRV
    ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1); // Velocity UAV

    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // Temperature SRV
    ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2); // Temperature UAV

    ranges[6].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        static_cast<UINT>(BufferSrvIndex::NumberOfSrvSlots) - 3,
        3); // t3+

    ranges[7].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        static_cast<UINT>(BufferUavIndex::NumberOfUavSlots) - 3,
        3); // u3+

    CD3DX12_ROOT_PARAMETER rootParams[9];

    for (int i = 0; i < 8; ++i)
    {
        rootParams[i].InitAsDescriptorTable(
            1, &ranges[i], D3D12_SHADER_VISIBILITY_ALL);
    }

    rootParams[8].InitAsConstantBufferView(0); // b0

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        _countof(rootParams),
        rootParams,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    winrt::com_ptr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        blob.put(),
        error.put()));

    ThrowIfFailed(device->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(m_rootSignature.put())));
}

void SimulationSystem::CreateSimulationKernels()
{
    GPUSorting::DeviceInfo devInfo = RenderSubsystem::GetDeviceInfo();
    // std::vector<std::wstring> compileArgs = {
    //     DXC_ARG_DEBUG,
    //     DXC_ARG_SKIP_OPTIMIZATIONS,
    //     L"-Qembed_debug"};
    std::vector<std::wstring> compileArgs = {DXC_ARG_OPTIMIZATION_LEVEL3};

    auto devicePtr = RenderSubsystem::GetDevice();
    std::filesystem::path shaderBase = L"shaders/simulation";

    m_predictPositions = std::make_unique<SimulationKernels::PredictPositions>(
        devicePtr, devInfo, compileArgs, shaderBase / L"1_PredictPositions.hlsl", m_rootSignature);

    m_collisionProjection = std::make_unique<SimulationKernels::CollisionProjection>(
        devicePtr, devInfo, compileArgs, shaderBase / L"2_CollisionProjection.hlsl", m_rootSignature);

    m_cellHash = std::make_unique<SimulationKernels::CellHash>(
        devicePtr, devInfo, compileArgs, shaderBase / L"3_CellHash.hlsl", m_rootSignature);

    m_hashToIndex = std::make_unique<SimulationKernels::HashToIndex>(
        devicePtr, devInfo, compileArgs, shaderBase / L"4_HashToIndex.hlsl", m_rootSignature);

    m_computeDensity = std::make_unique<SimulationKernels::ComputeDensity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"5_ComputeDensity.hlsl", m_rootSignature);

    m_computeLambda = std::make_unique<SimulationKernels::ComputeLambda>(
        devicePtr, devInfo, compileArgs, shaderBase / L"6_ComputeLambda.hlsl", m_rootSignature);

    m_computeDeltaPos = std::make_unique<SimulationKernels::ComputeDeltaPos>(
        devicePtr, devInfo, compileArgs, shaderBase / L"7_ComputeDeltaPos.hlsl", m_rootSignature);

    m_applyDeltaPos = std::make_unique<SimulationKernels::ApplyDeltaPos>(
        devicePtr, devInfo, compileArgs, shaderBase / L"8_ApplyDeltaPos.hlsl", m_rootSignature);

    m_updatePosVel = std::make_unique<SimulationKernels::UpdatePositionVelocity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"9_UpdatePositionVelocity.hlsl", m_rootSignature);

    m_viscosity = std::make_unique<SimulationKernels::Viscosity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"10_Viscosity.hlsl", m_rootSignature);

    m_applyViscosity = std::make_unique<SimulationKernels::ApplyViscosity>(
        devicePtr, devInfo, compileArgs, shaderBase / L"11_ApplyViscosity.hlsl", m_rootSignature);

    m_heatTransfer = std::make_unique<SimulationKernels::HeatTransfer>(
        devicePtr, devInfo, compileArgs, shaderBase / L"12_HeatTransfer.hlsl", m_rootSignature);

    m_oneSweep = std::make_unique<OneSweep>(devicePtr, devInfo, GPUSorting::ORDER_ASCENDING, GPUSorting::KEY_UINT32, GPUSorting::PAYLOAD_UINT32);
    m_oneSweep->SetAllBuffers(
        sortBuffers.hashBuffers[0]->resource,
        sortBuffers.indexBuffers[0]->resource,
        sortBuffers.hashBuffers[1]->resource,
        sortBuffers.indexBuffers[1]->resource,
        true);
    m_oneSweep->UpdateSize(m_maxParticlesCount, false);
}

// TODO: move to some utility file
inline std::shared_ptr<StructuredBuffer>
CreateBuffer(
    ID3D12Device *device,
    UINT count,
    UINT stride)
{
    auto buf = std::make_shared<StructuredBuffer>();
    buf->Init(device, count, stride);
    return buf;
}

void SimulationSystem::InitSimulationBuffers(
    ID3D12Device *device,
    DescriptorAllocator &allocGPU,
    UINT numParticles,
    UINT numCells)
{
    m_srvBase = allocGPU.AllocRange(static_cast<UINT>(BufferSrvIndex::NumberOfSrvSlots));
    m_uavBase = allocGPU.AllocRange(static_cast<UINT>(BufferUavIndex::NumberOfUavSlots));

    // TODO: some memory is unused but i dont care
    m_pingPongSrvBase = allocGPU.AllocRange(static_cast<UINT>(BufferSrvIndex::NumberOfSrvSlots));
    m_pingPongUavBase = allocGPU.AllocRange(static_cast<UINT>(BufferUavIndex::NumberOfUavSlots));

    for (int i = 0; i < 2; ++i)
    {
        particleSwapBuffers.position.buffers[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.velocity.buffers[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.temperature.buffers[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(float));
    }

    particleScratchBuffers.predictedPosition = CreateBuffer(
        device,
        numParticles,
        sizeof(DirectX::SimpleMath::Vector3));

    particleScratchBuffers.density = CreateBuffer(
        device,
        numParticles,
        sizeof(float));

    particleScratchBuffers.constraintC = CreateBuffer(
        device,
        numParticles,
        sizeof(float));

    particleScratchBuffers.lambda = CreateBuffer(
        device,
        numParticles,
        sizeof(float));

    particleScratchBuffers.deltaP = CreateBuffer(
        device,
        numParticles,
        sizeof(DirectX::SimpleMath::Vector3));

    particleScratchBuffers.viscosityMu = CreateBuffer(
        device,
        numParticles,
        sizeof(float));

    particleScratchBuffers.viscosityCoeff = CreateBuffer(
        device,
        numParticles,
        sizeof(float));

    particleScratchBuffers.cellStart = CreateBuffer(
        device,
        numCells,
        sizeof(uint32_t));

    particleScratchBuffers.cellEnd = CreateBuffer(
        device,
        numCells,
        sizeof(uint32_t));

    particleScratchBuffers.phase = CreateBuffer(
        device,
        numParticles,
        sizeof(uint32_t));

    for (int i = 0; i < 2; ++i)
    {
        sortBuffers.hashBuffers[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(uint32_t));

        sortBuffers.indexBuffers[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(uint32_t));
    }

    particleSwapBuffers.position.buffers[0]->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::Position);
    particleSwapBuffers.position.buffers[0]->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::Position);

    particleScratchBuffers.predictedPosition->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::PredictedPosition);
    particleScratchBuffers.predictedPosition->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::PredictedPosition);

    particleSwapBuffers.velocity.buffers[0]->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::Velocity);
    particleSwapBuffers.velocity.buffers[0]->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::Velocity);

    sortBuffers.hashBuffers[0]->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::ParticleHash);
    sortBuffers.hashBuffers[0]->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::ParticleHash);
    sortBuffers.indexBuffers[0]->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::SortedIndices);
    sortBuffers.indexBuffers[0]->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::SortedIndices);
    particleScratchBuffers.cellStart->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::CellStart);
    particleScratchBuffers.cellStart->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::CellStart);
    particleScratchBuffers.cellEnd->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::CellEnd);
    particleScratchBuffers.cellEnd->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::CellEnd);

    particleSwapBuffers.temperature.buffers[0]->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::Temperature);
    particleSwapBuffers.temperature.buffers[0]->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::Temperature);

    particleScratchBuffers.density->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::Density);
    particleScratchBuffers.density->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::Density);

    particleScratchBuffers.constraintC->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::ConstraintC);
    particleScratchBuffers.constraintC->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::ConstraintC);

    particleScratchBuffers.lambda->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::Lambda);
    particleScratchBuffers.lambda->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::Lambda);

    particleScratchBuffers.deltaP->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::DeltaP);
    particleScratchBuffers.deltaP->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::DeltaP);

    particleScratchBuffers.viscosityMu->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::ViscosityMu);
    particleScratchBuffers.viscosityMu->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::ViscosityMu);

    particleScratchBuffers.viscosityCoeff->CreateSRV(device, allocGPU, m_srvBase + BufferSrvIndex::ViscosityCoeff);
    particleScratchBuffers.viscosityCoeff->CreateUAV(device, allocGPU, m_uavBase + BufferUavIndex::ViscosityCoeff);

    // swap buffers
    particleSwapBuffers.position.buffers[1]->CreateSRV(device, allocGPU, m_pingPongSrvBase + BufferSrvIndex::Position);
    particleSwapBuffers.position.buffers[1]->CreateUAV(device, allocGPU, m_pingPongUavBase + BufferUavIndex::Position);

    particleSwapBuffers.temperature.buffers[1]->CreateSRV(device, allocGPU, m_pingPongSrvBase + BufferSrvIndex::Temperature);
    particleSwapBuffers.temperature.buffers[1]->CreateUAV(device, allocGPU, m_pingPongUavBase + BufferUavIndex::Temperature);

    particleSwapBuffers.velocity.buffers[1]->CreateSRV(device, allocGPU, m_pingPongSrvBase + BufferSrvIndex::Velocity);
    particleSwapBuffers.velocity.buffers[1]->CreateUAV(device, allocGPU, m_pingPongUavBase + BufferUavIndex::Velocity);

    InitTemperatureBuffer(device, numParticles);
}

void SimulationSystem::InitSortIndexBuffers(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles)
{
    std::vector<uint32_t> hostIndices(m_maxParticlesCount);
    std::iota(hostIndices.begin(), hostIndices.end(), 0u);

    UINT64 uploadSize = UINT64(m_maxParticlesCount) * sizeof(uint32_t);

    auto uploadResource = UploadHelpers::CreateUploadBuffer(device, uploadSize);

    void *pUpload = nullptr;
    D3D12_RANGE readRange{0, 0}; // CPU write-only
    ThrowIfFailed(uploadResource->Map(0, &readRange, &pUpload));
    memcpy(pUpload, hostIndices.data(), uploadSize);
    uploadResource->Unmap(0, nullptr);

    winrt::com_ptr<ID3D12CommandAllocator> cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.put())));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.get(), nullptr, IID_PPV_ARGS(cmdList.put())));

    auto dstRes = sortBuffers.indexBuffers[0]->resource.get();
    UploadHelpers::CopyBufferToResource(cmdList.get(), uploadResource.get(), dstRes, uploadSize, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    auto dstRes2 = sortBuffers.indexBuffers[1]->resource.get();
    UploadHelpers::CopyBufferToResource(cmdList.get(), uploadResource.get(), dstRes2, uploadSize, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists[] = {cmdList.get()};
    auto queue = RenderSubsystem::GetCommandQueue();
    queue->ExecuteCommandLists(1, lists);

    // fence and wait
    winrt::com_ptr<ID3D12Fence> fence;
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
    uint64_t fenceVal = 1;
    RenderSubsystem::WaitForFence(fence.get(), fenceVal);
}

void SimulationSystem::InitTemperatureBuffer(ID3D12Device *device, UINT numParticles)
{
    // prepare host data filled with 900.0f
    std::vector<float> hostTemps(numParticles, 900.0f);

    UINT64 uploadSize = UINT64(numParticles) * sizeof(float);
    auto uploadResource = UploadHelpers::CreateUploadBuffer(device, uploadSize);

    void *pUpload = nullptr;
    D3D12_RANGE readRange{0, 0};
    ThrowIfFailed(uploadResource->Map(0, &readRange, &pUpload));
    memcpy(pUpload, hostTemps.data(), uploadSize);
    uploadResource->Unmap(0, nullptr);

    winrt::com_ptr<ID3D12CommandAllocator> cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.put())));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.get(), nullptr, IID_PPV_ARGS(cmdList.put())));

    // copy into both temperature swap buffers
    UploadHelpers::CopyBufferToResource(cmdList.get(), uploadResource.get(), particleSwapBuffers.temperature.buffers[0]->resource.get(), uploadSize, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    UploadHelpers::CopyBufferToResource(cmdList.get(), uploadResource.get(), particleSwapBuffers.temperature.buffers[1]->resource.get(), uploadSize, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists[] = {cmdList.get()};
    auto queue = RenderSubsystem::GetCommandQueue();
    queue->ExecuteCommandLists(1, lists);

    // fence and wait
    winrt::com_ptr<ID3D12Fence> fence;
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
    uint64_t fenceVal = 1;
    RenderSubsystem::WaitForFence(fence.get(), fenceVal);
}
#pragma endregion

// TODO: move somewhere
#pragma region ROOT SIGNATURE SETTERS
void SimulationSystem::SetPingPongBufferRootSig(
    ID3D12GraphicsCommandList *cmdList,
    const PingPongBuffer &buffer,
    UINT rootSrvIndex,
    UINT rootUavIndex,
    DescriptorAllocator &allocGPU)
{
    auto readBuf = buffer.GetReadBuffer();
    auto writeBuf = buffer.GetWriteBuffer();

    cmdList->SetComputeRootDescriptorTable(
        rootSrvIndex,
        allocGPU.GetGpuHandle(readBuf->srvIndex));

    cmdList->SetComputeRootDescriptorTable(
        rootUavIndex,
        allocGPU.GetGpuHandle(writeBuf->uavIndex));
}

void SimulationSystem::SetPositionPingPongRootSig(
    ID3D12GraphicsCommandList *cmdList,
    DescriptorAllocator &allocGPU)
{
    // SetPingPongBufferRootSig(
    //     cmdList,
    //     particleSwapBuffers.position,
    //     0, // root SRV
    //     1, // root UAV
    //     allocGPU);

    auto buf = particleSwapBuffers.position.GetReadBuffer();

    cmdList->SetComputeRootDescriptorTable(
        0,
        allocGPU.GetGpuHandle(buf->srvIndex));

    cmdList->SetComputeRootDescriptorTable(
        1,
        allocGPU.GetGpuHandle(buf->uavIndex));
}

void SimulationSystem::SetVelocityPingPongRootSig(
    ID3D12GraphicsCommandList *cmdList,
    DescriptorAllocator &allocGPU)
{
    SetPingPongBufferRootSig(
        cmdList,
        particleSwapBuffers.velocity,
        2, // root SRV
        3, // root UAV
        allocGPU);
}

void SimulationSystem::SetTemperaturePingPongRootSig(
    ID3D12GraphicsCommandList *cmdList,
    DescriptorAllocator &allocGPU)
{
    SetPingPongBufferRootSig(
        cmdList,
        particleSwapBuffers.temperature,
        4, // root SRV
        5, // root UAV
        allocGPU);
}

void SimulationSystem::SetOtherSrvsRootSig(
    ID3D12GraphicsCommandList *cmdList,
    DescriptorAllocator &allocGPU)
{
    UINT baseIndex = sortBuffers.hashBuffers[0]->srvIndex;

    cmdList->SetComputeRootDescriptorTable(
        6,
        allocGPU.GetGpuHandle(baseIndex));
}

void SimulationSystem::SetOtherUavsRootSig(
    ID3D12GraphicsCommandList *cmdList,
    DescriptorAllocator &allocGPU)
{
    UINT baseIndex = sortBuffers.hashBuffers[0]->uavIndex;

    cmdList->SetComputeRootDescriptorTable(
        7,
        allocGPU.GetGpuHandle(baseIndex));
}

void SimulationSystem::SetSimulationConstantRootSig(
    ID3D12GraphicsCommandList *cmdList,
    D3D12_GPU_VIRTUAL_ADDRESS cbAddress)
{
    cmdList->SetComputeRootConstantBufferView(8, cbAddress);
}

void SimulationSystem::SetRootSigAndDescTables(ID3D12GraphicsCommandList *cmdList, DescriptorAllocator &allocGPU)
{
    ID3D12DescriptorHeap *heaps[] = {allocGPU.GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetComputeRootSignature(m_rootSignature.get());

    SetPositionPingPongRootSig(cmdList, allocGPU);
    SetVelocityPingPongRootSig(cmdList, allocGPU);
    SetTemperaturePingPongRootSig(cmdList, allocGPU);

    SetOtherSrvsRootSig(cmdList, allocGPU);
    SetOtherUavsRootSig(cmdList, allocGPU);

    SetSimulationConstantRootSig(
        cmdList,
        m_simParamsUpload->GetGPUVirtualAddress());
}
#pragma endregion

#pragma region SIMULATE
static winrt::com_ptr<ID3D12Fence> fence = nullptr;
void SimulationSystem::Simulate(float dt)
{
    if (!isRunning)
    {
        return;
    }

    winrt::com_ptr<ID3D12Device> device = RenderSubsystem::GetDevice();

    static int fenceVal;

    if (!fence)
    {
        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
    }

    m_simParams.dt = dt;

    // copy updated SimParams into the upload constant buffer
    D3D12_RANGE readRange{0, 0};
    void *pData = nullptr;
    ThrowIfFailed(m_simParamsUpload->Map(0, &readRange, &pData));
    memcpy(pData, &m_simParams, sizeof(SimParams));
    m_simParamsUpload->Unmap(0, nullptr);

    std::shared_ptr<DescriptorAllocator> allocGPU = RenderSubsystem::GetCBVSRVUAVAllocatorGPUVisible();

    // TODO: do not create allocator and list each frame
    winrt::com_ptr<ID3D12CommandAllocator> cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.put())));
    ThrowIfFailed(cmdAlloc->Reset());
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.get(), nullptr, IID_PPV_ARGS(cmdList.put())));

    SetRootSigAndDescTables(cmdList.get(), *allocGPU);

    const uint32_t numParticles = static_cast<uint32_t>(m_simParams.numParticles);

    // 1) Predict positions
    m_predictPositions->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleScratchBuffers.predictedPosition->resource);

    // 2) Simple collision projection (in-place on predicted/velocity buffers)
    m_collisionProjection->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleScratchBuffers.predictedPosition->resource);

    // 3) Compute spatial hash into sort buffers
    m_cellHash->Dispatch(cmdList, numParticles);
    // UAVBarrierSingle(cmdList, sortBuffers.hashBuffers[0]->resource);

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists[] = {cmdList.get()};
    auto queue = RenderSubsystem::GetCommandQueue();
    queue->ExecuteCommandLists(1, lists);
    ThrowIfFailed(cmdList->Reset(cmdAlloc.get(), nullptr));

    fenceVal++;
    RenderSubsystem::WaitForFence(fence.get(), fenceVal);

    // TODO: sort to this command list
    // 4) Configure OneSweep to use our hash/index buffers and sort
    m_oneSweep->Sort();

    fenceVal++;
    RenderSubsystem::WaitForFence(fence.get(), fenceVal);

    SetRootSigAndDescTables(cmdList.get(), *allocGPU);

    // 5) (hash->cell start)
    m_hashToIndex->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleScratchBuffers.cellStart->resource);
    UAVBarrierSingle(cmdList, particleScratchBuffers.cellEnd->resource);

    // 6) PBF solver iterations
    for (int iter = 0; iter < 10; ++iter)
    {
        // Compute density
        m_computeDensity->Dispatch(cmdList, numParticles);
        UAVBarrierSingle(cmdList, particleScratchBuffers.density->resource);

        // Compute lambda
        m_computeLambda->Dispatch(cmdList, numParticles);
        UAVBarrierSingle(cmdList, particleScratchBuffers.lambda->resource);

        // Compute position corrections
        m_computeDeltaPos->Dispatch(cmdList, numParticles);
        UAVBarrierSingle(cmdList, particleScratchBuffers.deltaP->resource);

        // Apply corrections
        m_applyDeltaPos->Dispatch(cmdList, numParticles);
        UAVBarrierSingle(cmdList, particleScratchBuffers.predictedPosition->resource);
    }

    // 7) Update positions and velocities (write to position and velocity dst buffers)
    m_updatePosVel->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleSwapBuffers.position.GetWriteBuffer()->resource);
    UAVBarrierSingle(cmdList, particleSwapBuffers.velocity.GetWriteBuffer()->resource);

    // 8) Viscosity: compute viscosity mu and coefficient from temperature
    m_viscosity->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleScratchBuffers.viscosityCoeff->resource);

    // 9) Apply viscosity to velocities
    m_applyViscosity->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleSwapBuffers.velocity.GetWriteBuffer()->resource);
    particleSwapBuffers.velocity.Swap();
    // SetVelocityPingPongRootSig(cmdList.get(), *allocGPU);

    // 10) Heat transfer (temperature diffusion)
    m_heatTransfer->Dispatch(cmdList, numParticles);
    UAVBarrierSingle(cmdList, particleSwapBuffers.temperature.GetWriteBuffer()->resource);
    particleSwapBuffers.temperature.Swap();

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList *lists2[] = {cmdList.get()};
    queue->ExecuteCommandLists(1, lists2);
    ThrowIfFailed(cmdList->Reset(cmdAlloc.get(), nullptr));

    fenceVal++;
    RenderSubsystem::WaitForFence(fence.get(), fenceVal);
}
#pragma endregion

// TODO: move to StructuredBuffer?
#pragma region GETTERS
D3D12_GPU_DESCRIPTOR_HANDLE SimulationSystem::GetPositionBufferSRV()
{
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocatorGPUVisible();
    UINT idx = particleSwapBuffers.position.GetReadBuffer()->srvIndex;
    return alloc->GetGpuHandle(idx);
}

D3D12_GPU_DESCRIPTOR_HANDLE SimulationSystem::GetTemperatureBufferSRV()
{
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocatorGPUVisible();
    UINT idx = particleSwapBuffers.temperature.GetReadBuffer()->srvIndex;
    return alloc->GetGpuHandle(idx);
}
#pragma endregion

#pragma region UTILITY
UINT operator+(UINT offset, BufferSrvIndex index) { return offset + static_cast<UINT>(index); };
UINT operator+(BufferSrvIndex index, UINT offset) { return static_cast<UINT>(index) + offset; };
UINT operator+(UINT offset, BufferUavIndex index) { return offset + static_cast<UINT>(index); };
UINT operator+(BufferUavIndex index, UINT offset) { return static_cast<UINT>(index) + offset; };
#pragma endregion