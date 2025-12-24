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
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    // Allocate fraction of particles to sphere at top-center
    const UINT sphereCount = static_cast<UINT>(std::round(numParticles * 0.18f));
    const UINT bottomCount = numParticles - sphereCount;

    // Dense bottom: bias y downward using pow to cluster near 0
    for (UINT i = 0; i < bottomCount; ++i)
    {
        float x = u(rng);
        float z = u(rng);
        // bias towards bottom (y in [0,1], 0 = bottom)
        float y = std::pow(u(rng), 2.0f); // square to bias low
        out.emplace_back(x, y, z);
    }

    // Sphere at top-center
    DirectX::SimpleMath::Vector3 center(0.5f, 0.82f, 0.5f);
    const float radius = 0.12f;
    std::uniform_real_distribution<float> uSphere(-radius, radius);
    while (out.size() < numParticles)
    {
        float rx = uSphere(rng);
        float ry = uSphere(rng);
        float rz = uSphere(rng);
        if (rx * rx + ry * ry + rz * rz <= radius * radius)
        {
            DirectX::SimpleMath::Vector3 p = center + DirectX::SimpleMath::Vector3(rx, ry, rz);
            // clamp to [0,1]
            p.x = std::min(1.0f, std::max(0.0f, p.x));
            p.y = std::min(1.0f, std::max(0.0f, p.y));
            p.z = std::min(1.0f, std::max(0.0f, p.z));
            out.push_back(p);
        }
    }

    return out;
}
// TODO: cringe but okay for now

void SimulationSystem::GenerateTemperaturesForPositions(const std::vector<DirectX::SimpleMath::Vector3> &positions, std::vector<float> &outTemps)
{
    outTemps.clear();
    outTemps.reserve(positions.size());

    std::mt19937 rngTemp(424242);
    std::uniform_real_distribution<float> d700_1000(700.0f, 1000.0f);
    std::uniform_real_distribution<float> d900_1000(900.0f, 1000.0f);
    std::uniform_real_distribution<float> d1300_1400(1300.0f, 1400.0f);

    // Detect if this is the dense-bottom-with-sphere scene by presence of points inside the known sphere region
    DirectX::SimpleMath::Vector3 sphereCenter(0.5f, 0.82f, 0.5f);
    const float sphereRadius = 0.12f;
    bool hasSphere = false;
    for (const auto &p : positions)
    {
        float dx = p.x - sphereCenter.x;
        float dy = p.y - sphereCenter.y;
        float dz = p.z - sphereCenter.z;
        if (dx * dx + dy * dy + dz * dz <= sphereRadius * sphereRadius)
        {
            hasSphere = true;
            break;
        }
    }

    if (hasSphere)
    {
        for (const auto &p : positions)
        {
            float dx = p.x - sphereCenter.x;
            float dy = p.y - sphereCenter.y;
            float dz = p.z - sphereCenter.z;
            if (dx * dx + dy * dy + dz * dz <= sphereRadius * sphereRadius)
                outTemps.push_back(d1300_1400(rngTemp));
            else if (p.y < 0.45f)
                outTemps.push_back(d900_1000(rngTemp));
            else
                outTemps.push_back(d700_1000(rngTemp));
        }
    }
    else
    {
        for (size_t i = 0; i < positions.size(); ++i)
            outTemps.push_back(d700_1000(rngTemp));
    }
}

#pragma region INIT
void SimulationSystem::Init(ID3D12Device *device)
{
    // initialize simulation buffers (SRV/UAV descriptors)
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocator();
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

    // Generate initial particle positions. Use one of the helper generators below.
    // Options:
    //  - GenerateUniformGridPositions(m_maxParticlesCount)
    //  - GenerateDenseRandomPositions(m_maxParticlesCount)
    //  - GenerateDenseBottomWithSphere(m_maxParticlesCount)  <-- default
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
        particleSwapBuffers.position[0]->resource.get(),
        uploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // copy into second swap buffer (COMMON -> COPY -> UAV)
    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadResource.get(),
        particleSwapBuffers.position[1]->resource.get(),
        uploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // copy temperature upload into both temperature swap buffers
    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadTempResource.get(),
        particleSwapBuffers.temperature[0]->resource.get(),
        tempUploadSize,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    UploadHelpers::CopyBufferToResource(
        cmdList.get(),
        uploadTempResource.get(),
        particleSwapBuffers.temperature[1]->resource.get(),
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
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        static_cast<UINT>(BufferSrvIndex::NumberOfSrvSlots),
        0);

    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        static_cast<UINT>(BufferUavIndex::NumberOfUavSlots),
        0);

    CD3DX12_ROOT_PARAMETER rootParams[3];

    rootParams[0].InitAsDescriptorTable(
        1, &srvRange,
        D3D12_SHADER_VISIBILITY_ALL);

    rootParams[1].InitAsDescriptorTable(
        1, &uavRange,
        D3D12_SHADER_VISIBILITY_ALL);

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

void SimulationSystem::CreateSimulationKernels()
{
    GPUSorting::DeviceInfo devInfo = RenderSubsystem::GetDeviceInfo();
    std::vector<std::wstring> compileArgs = {
        DXC_ARG_DEBUG,
        DXC_ARG_SKIP_OPTIMIZATIONS,
        L"-Qembed_debug"};

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
    DescriptorAllocator &alloc,
    UINT numParticles,
    UINT numCells)
{
    // Reserve descriptor table ranges that match the root signature
    m_srvBase = alloc.AllocRange(static_cast<UINT>(BufferSrvIndex::NumberOfSrvSlots));
    m_uavBase = alloc.AllocRange(static_cast<UINT>(BufferUavIndex::NumberOfUavSlots));

    // Create resources (no descriptors yet)
    for (int i = 0; i < 2; ++i)
    {
        particleSwapBuffers.position[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.predictedPosition[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.velocity[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(DirectX::SimpleMath::Vector3));

        particleSwapBuffers.temperature[i] = CreateBuffer(
            device,
            numParticles,
            sizeof(float));
    }

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

    // Create SRV/UAV descriptors using the reserved ranges.
    particleSwapBuffers.position[0]->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::Position0);
    particleSwapBuffers.position[0]->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::Position0);

    particleSwapBuffers.position[1]->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::Position1);
    particleSwapBuffers.position[1]->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::Position1);

    particleSwapBuffers.velocity[0]->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::Velocity);
    particleSwapBuffers.velocity[0]->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::Velocity);

    sortBuffers.hashBuffers[0]->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::ParticleHash);
    sortBuffers.hashBuffers[0]->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::ParticleHash);
    sortBuffers.indexBuffers[0]->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::SortedIndices);
    sortBuffers.indexBuffers[0]->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::SortedIndices);
    particleScratchBuffers.cellStart->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::CellStart);
    particleScratchBuffers.cellStart->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::CellStart);
    particleScratchBuffers.cellEnd->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::CellEnd);
    particleScratchBuffers.cellEnd->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::CellEnd);

    particleSwapBuffers.temperature[0]->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::Temperature);
    particleSwapBuffers.temperature[0]->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::Temperature);

    particleScratchBuffers.density->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::Density);
    particleScratchBuffers.density->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::Density);

    particleScratchBuffers.constraintC->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::ConstraintC);
    particleScratchBuffers.constraintC->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::ConstraintC);

    particleScratchBuffers.lambda->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::Lambda);
    particleScratchBuffers.lambda->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::Lambda);

    particleScratchBuffers.deltaP->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::DeltaP);
    particleScratchBuffers.deltaP->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::DeltaP);

    particleScratchBuffers.viscosityMu->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::ViscosityMu);
    particleScratchBuffers.viscosityMu->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::ViscosityMu);

    particleScratchBuffers.viscosityCoeff->CreateSRV(device, alloc, m_srvBase + BufferSrvIndex::ViscosityCoeff);
    particleScratchBuffers.viscosityCoeff->CreateUAV(device, alloc, m_uavBase + BufferUavIndex::ViscosityCoeff);
    // initialize temperature buffer with default value (900)
    InitTemperatureBuffer(device, alloc, numParticles);
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

void SimulationSystem::InitTemperatureBuffer(ID3D12Device *device, DescriptorAllocator &alloc, UINT numParticles)
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
    UploadHelpers::CopyBufferToResource(cmdList.get(), uploadResource.get(), particleSwapBuffers.temperature[0]->resource.get(), uploadSize, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    UploadHelpers::CopyBufferToResource(cmdList.get(), uploadResource.get(), particleSwapBuffers.temperature[1]->resource.get(), uploadSize, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

#pragma region SIMULATE
void SimulationSystem::Simulate(float dt)
{
    // update time step
    m_simParams.dt = dt;

    // copy updated SimParams into the upload constant buffer
    D3D12_RANGE readRange{0, 0};
    void *pData = nullptr;
    ThrowIfFailed(m_simParamsUpload->Map(0, &readRange, &pData));
    memcpy(pData, &m_simParams, sizeof(SimParams));
    m_simParamsUpload->Unmap(0, nullptr);

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

    cmdList->SetComputeRootSignature(m_rootSignature.get());

    cmdList->SetComputeRootDescriptorTable(0, alloc->GetGpuHandle(m_srvBase));
    cmdList->SetComputeRootDescriptorTable(1, alloc->GetGpuHandle(m_uavBase));
    cmdList->SetComputeRootConstantBufferView(2, m_simParamsUpload->GetGPUVirtualAddress());

    const uint32_t numParticles = static_cast<uint32_t>(m_simParams.numParticles);

    std::swap(particleSwapBuffers.position[0], particleSwapBuffers.position[1]);

    // 1) Predict positions
    m_predictPositions->Dispatch(cmdList, numParticles);

    // 2) Simple collision projection (in-place on predicted/velocity buffers)
    m_collisionProjection->Dispatch(cmdList, numParticles);

    // 3) Compute spatial hash into sort buffers (use first pair)
    m_cellHash->Dispatch(cmdList, numParticles);

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
    m_oneSweep->Sort();

    // TODO: probaply one list enough?
    ThrowIfFailed(cmdAlloc->Reset());
    ThrowIfFailed(cmdList->Reset(cmdAlloc.get(), nullptr));
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetComputeRootSignature(m_rootSignature.get());

    cmdList->SetComputeRootDescriptorTable(0, alloc->GetGpuHandle(m_srvBase));
    cmdList->SetComputeRootDescriptorTable(1, alloc->GetGpuHandle(m_uavBase));
    cmdList->SetComputeRootConstantBufferView(2, m_simParamsUpload->GetGPUVirtualAddress());

    // 5) (hash->cell start)
    m_hashToIndex->Dispatch(cmdList, numParticles);

    // 6) PBF solver iterations
    for (int iter = 0; iter < 5; ++iter)
    {
        // Compute density
        m_computeDensity->Dispatch(cmdList, numParticles);

        // Compute lambda (constraint multipliers)
        m_computeLambda->Dispatch(cmdList, numParticles);

        // Compute position corrections
        m_computeDeltaPos->Dispatch(cmdList, numParticles);

        // Apply corrections (in-place on predicted positions)
        m_applyDeltaPos->Dispatch(cmdList, numParticles);
    }

    // 7) Update positions and velocities (write to position and velocity dst buffers)
    m_updatePosVel->Dispatch(cmdList, numParticles);

    // TODO: enable
    // 8) Viscosity: compute viscosity mu and coefficient from temperature
    // m_viscosity->Dispatch(cmdList, numParticles);

    // TODO: enable
    // 9) Apply viscosity to velocities
    // m_applyViscosity->Dispatch(cmdList, numParticles);

    // 10) Heat transfer (temperature diffusion)
    m_heatTransfer->Dispatch(cmdList, numParticles);

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

// TODO: move to StructuredBuffer?
#pragma region GETTERS
D3D12_GPU_DESCRIPTOR_HANDLE SimulationSystem::GetPositionBufferSRV()
{
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocator();
    UINT idx = particleSwapBuffers.position[m_currentSwapIndex]->srvIndex;
    return alloc->GetGpuHandle(idx);
}

D3D12_GPU_DESCRIPTOR_HANDLE SimulationSystem::GetTemperatureBufferSRV()
{
    auto alloc = RenderSubsystem::GetCBVSRVUAVAllocator();
    UINT idx = particleSwapBuffers.temperature[m_currentSwapIndex]->srvIndex;
    return alloc->GetGpuHandle(idx);
}
#pragma endregion

#pragma region UTILITY
UINT operator+(UINT offset, BufferSrvIndex index) { return offset + static_cast<UINT>(index); };
UINT operator+(BufferSrvIndex index, UINT offset) { return static_cast<UINT>(index) + offset; };
UINT operator+(UINT offset, BufferUavIndex index) { return offset + static_cast<UINT>(index); };
UINT operator+(BufferUavIndex index, UINT offset) { return static_cast<UINT>(index) + offset; };
#pragma endregion