#include "ParticleSystem.h"
#include "RenderTemplatesAPI.h"
#include "ShaderCompiler.h"
#include "RenderSubsystem.h"
#include <stdexcept>
#include <vector>
#include <random>
#include <cstring>

void ParticleSystem::Init(ID3D12Device *device)
{
    assert(device && "ParticleSystem::Init requires a valid ID3D12Device");

    const uint64_t count = static_cast<uint64_t>(m_maxParticlesCount);

    const uint64_t vec4Size = sizeof(float) * 4;
    const uint64_t bufSizeVec4 = vec4Size * count;

    const uint64_t floatSize = sizeof(float);
    const uint64_t bufSizeFloat = floatSize * count;

    D3D12_HEAP_PROPERTIES heapProps = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC descVec4 = GetBufferResourceDesc(bufSizeVec4);
    descVec4.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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

    // --- Create buffers for sorting ---
    const uint64_t elemSize = sizeof(SortElement);
    const uint64_t sortBufSize = elemSize * count;

    D3D12_RESOURCE_DESC sortDesc = GetBufferResourceDesc(sortBufSize);
    sortDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &sortDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&sortBuffers[0]));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create sort buffer 0");
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &sortDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&sortBuffers[1]));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create sort buffer 1");

    // blockCounts and prefix buffer (NumBlocks * 2 uints) - create as default buffer with UAV
    uint32_t NumBlocks = static_cast<uint32_t>((count + 255) / 256);
    uint32_t NumCounts = NumBlocks * 2;
    uint64_t countsSize = sizeof(uint32_t) * NumCounts;

    D3D12_RESOURCE_DESC countsDesc = GetBufferResourceDesc(countsSize);
    countsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &countsDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&blockCounts));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create blockCounts buffer");
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &countsDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&prefixBuffer));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create prefix buffer");

    // Readback for blockCounts
    D3D12_HEAP_PROPERTIES readbackHeap = MakeHeapProperties(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC readbackDesc = GetBufferResourceDesc(countsSize);
    readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&blockCountsReadback));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create blockCounts readback buffer");

    // Upload buffer for prefix
    D3D12_HEAP_PROPERTIES uploadHeap = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC uploadDesc = GetBufferResourceDesc(countsSize);
    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&prefixUpload));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create prefix upload buffer");

    m_uavHeap = RenderSubsystem::GetCBVSRVUAVHeap();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_uavHeap->GetCPUDescriptorHandleForHeapStart();
    UINT increment = RenderSubsystem::GetCBVSRVUAVDescriptorSize();

    // TODO: something wrong here?
    // Input (u0)
    // Output (u1)
    cpuHandle.ptr += increment;
    // BlockCounts (u2) - NumCounts uints
    cpuHandle.ptr += increment;
    // Prefix (u3)
    cpuHandle.ptr += increment;
    // Create UAV descriptors using helpers
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = MakeBufferUAVDesc(static_cast<UINT>(count), static_cast<UINT>(elemSize), DXGI_FORMAT_UNKNOWN);
    device->CreateUnorderedAccessView(sortBuffers[0].Get(), nullptr, &uavDesc, cpuHandle);

    // Output (u1)
    cpuHandle.ptr += increment;
    device->CreateUnorderedAccessView(sortBuffers[1].Get(), nullptr, &uavDesc, cpuHandle);

    // BlockCounts (u2) - NumCounts uints
    cpuHandle.ptr += increment;
    D3D12_UNORDERED_ACCESS_VIEW_DESC bcDesc = MakeBufferUAVDesc(NumCounts, sizeof(uint32_t), DXGI_FORMAT_UNKNOWN);
    device->CreateUnorderedAccessView(blockCounts.Get(), nullptr, &bcDesc, cpuHandle);

    // Prefix (u3)
    cpuHandle.ptr += increment;
    device->CreateUnorderedAccessView(prefixBuffer.Get(), nullptr, &bcDesc, cpuHandle);

    // Create compute pipelines (root signature + PSOs)
    CreateSortPipelines(device);

    // Fill sortBuffers[0] with random test data (keys and indices)
    std::vector<uint32_t> initialData;
    initialData.resize(static_cast<size_t>(count) * 2);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<uint32_t> dist(0, 16);
    for (uint64_t i = 0; i < count; ++i)
    {
        initialData[i * 2 + 0] = dist(rng);                // key
        initialData[i * 2 + 1] = static_cast<uint32_t>(i); // particleIndex
    }

    // Create upload resource for initial data and copy into sortBuffers[0]
    D3D12_RESOURCE_DESC initDesc = GetBufferResourceDesc(sortBufSize);
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadInit;
    hr = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &initDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadInit));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create upload resource for initial sort data");

    // Map and copy
    uint8_t *mapPtr = nullptr;
    D3D12_RANGE r = {0, 0};
    uploadInit->Map(0, &r, reinterpret_cast<void **>(&mapPtr));
    memcpy(mapPtr, initialData.data(), static_cast<size_t>(sortBufSize));
    uploadInit->Unmap(0, nullptr);

    // Copy uploadInit -> sortBuffers[0] using a short command list and the renderer's command queue
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tmpAlloc;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tmpAlloc));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create temp alloc");
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tmpList;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tmpAlloc.Get(), nullptr, IID_PPV_ARGS(&tmpList));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create temp list");

    tmpList->CopyResource(sortBuffers[0].Get(), uploadInit.Get());
    tmpList->Close();

    ID3D12CommandQueue *rendererQueue = RenderSubsystem::GetCommandQueue();
    ID3D12CommandList *lists[] = {tmpList.Get()};
    rendererQueue->ExecuteCommandLists(1, lists);

    // Wait for upload to finish
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    uint64_t fv = 1;
    rendererQueue->Signal(fence.Get(), fv);
    if (fence->GetCompletedValue() < fv)
    {
        fence->SetEventOnCompletion(fv, ev);
        WaitForSingleObject(ev, INFINITE);
    }
    CloseHandle(ev);
}

// Create compute root signature and pipeline states for Count and Scatter kernels
void ParticleSystem::CreateSortPipelines(ID3D12Device *device)
{
    // descriptor ranges: UAVs u0-u3 (Input, Output, BlockCounts, Prefix)
    D3D12_DESCRIPTOR_RANGE ranges[1] = {CreateDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0)};

    D3D12_ROOT_PARAMETER rootParams[2] = {CreateDescriptorTableRootParam(&ranges[0], 1, D3D12_SHADER_VISIBILITY_ALL),
                                          CreateCBVRootParam(0, 0, D3D12_SHADER_VISIBILITY_ALL)};

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pParameters = rootParams;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(hr))
    {
        if (errors)
            OutputDebugStringA((char *)errors->GetBufferPointer());
        throw std::runtime_error("Failed to serialize root signature for sort pipelines");
    }

    Microsoft::WRL::ComPtr<ID3DBlob> serialized2;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized2, &errors);
    if (FAILED(hr))
        throw std::runtime_error("Failed to serialize root signature 2");
    hr = device->CreateRootSignature(0, serialized2->GetBufferPointer(), serialized2->GetBufferSize(), IID_PPV_ARGS(&ParticleSystem::m_csRootSig));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create compute root signature");

    // Compile compute shaders
    auto countResult = ShaderCompiler::CompileFromFile(
        L"c:\\Users\\jaro_\\source\\repos\\MastersLavaSimulation\\shaders\\RadixSort.hlsl",
        "CountKernel",
        "cs_5_0");
    if (!countResult.success)
    {
        if (countResult.error)
            OutputDebugStringA((char *)countResult.error->GetBufferPointer());
        throw std::runtime_error("Failed to compile CountKernel");
    }

    auto scatterResult = ShaderCompiler::CompileFromFile(
        L"c:\\Users\\jaro_\\source\\repos\\MastersLavaSimulation\\shaders\\RadixSort.hlsl",
        "ScatterKernel",
        "cs_5_0");
    if (!scatterResult.success)
    {
        if (scatterResult.error)
            OutputDebugStringA((char *)scatterResult.error->GetBufferPointer());
        throw std::runtime_error("Failed to compile ScatterKernel");
    }

    // Create PSO descs
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ParticleSystem::m_csRootSig.Get();
    psoDesc.CS = {countResult.bytecode->GetBufferPointer(), countResult.bytecode->GetBufferSize()};
    hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ParticleSystem::m_countPSO));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create count PSO");

    psoDesc.CS = {scatterResult.bytecode->GetBufferPointer(), scatterResult.bytecode->GetBufferSize()};
    hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ParticleSystem::m_scatterPSO));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create scatter PSO");
}

// CPU exclusive prefix sum
static void ExclusivePrefixCPU(uint32_t *data, uint32_t length, std::vector<uint32_t> &outPrefix)
{
    outPrefix.resize(length);
    uint64_t sum = 0;
    for (uint32_t i = 0; i < length; ++i)
    {
        outPrefix[i] = static_cast<uint32_t>(sum);
        sum += data[i];
    }
}

void ParticleSystem::SortOnce(ID3D12Device *device, ID3D12CommandQueue *queue, uint32_t NumElements)
{
    if (!device || !queue)
        throw std::runtime_error("SortOnce requires device and queue");

    // Compute block counts length: NumBlocks = ceil(NumElements / 256)
    const uint32_t THREADS = 256;
    uint32_t NumBlocks = (NumElements + THREADS - 1) / THREADS;
    uint32_t NumCounts = NumBlocks * 2; // zero/one per block

    // Create a command allocator and list for the sort work
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc;
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command allocator for sort");

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmdList));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command list for sort");

    ID3D12DescriptorHeap *heaps[] = {m_uavHeap};
    cmdList->SetDescriptorHeaps(1, heaps);

    // Run CountKernel
    cmdList->SetPipelineState(m_countPSO.Get());
    cmdList->SetComputeRootSignature(m_csRootSig.Get());
    // root table at 0 (uavs)
    cmdList->SetComputeRootDescriptorTable(0, m_uavHeap->GetGPUDescriptorHandleForHeapStart());
    // root CBV (params) will be set via root constant buffer later per-dispatch

    // For simplicity, allocate a small upload buffer for Params and copy it to a GPU upload buffer bound as CBV
    // Create an upload resource for params
    D3D12_HEAP_PROPERTIES upHeap = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC paramDesc = GetBufferResourceDesc(sizeof(uint32_t) * 4);
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsUpload;
    hr = device->CreateCommittedResource(&upHeap, D3D12_HEAP_FLAG_NONE, &paramDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&paramsUpload));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create params upload");

    for (uint32_t shift = 0; shift < 32; ++shift)
    {
        // TODO: вынести
        struct P
        {
            uint32_t g_Shift;
            uint32_t NumElements;
            uint32_t NumCounts;
            uint32_t totalZeros;
        } p;

        p.g_Shift = shift;
        p.NumElements = NumElements;
        p.NumCounts = NumCounts;
        p.totalZeros = 0; // will be set later

        UINT8 *mapped = nullptr;
        D3D12_RANGE r = {0, 0};
        paramsUpload->Map(0, &r, reinterpret_cast<void **>(&mapped));
        memcpy(mapped, &p, sizeof(p));
        paramsUpload->Unmap(0, nullptr);

        // Create a GPU-visible CBV for params in-place by using SetComputeRoot32BitConstants? Simpler: use SetComputeRootConstantBufferView
        // First we need a GPU buffer for params: copy upload directly into default buffer for CBV is more complex. Instead, we can use root constants: pass 4 uints via SetComputeRoot32BitConstants
        cmdList->SetComputeRoot32BitConstants(1, 4, &p, 0);

        // Dispatch CountKernel
        cmdList->Dispatch(NumBlocks, 1, 1);

        // TODO: use helper function for barriers
        // UAV barrier to ensure BlockCounts are written before copying/readback
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        uavBarrier.UAV.pResource = blockCounts.Get();
        cmdList->ResourceBarrier(1, &uavBarrier);

        // Copy blockCounts to readback buffer
        cmdList->CopyResource(blockCountsReadback.Get(), blockCounts.Get());

        // Close and execute
        cmdList->Close();
        ID3D12CommandList *lists[] = {cmdList.Get()};
        queue->ExecuteCommandLists(1, lists);

        // Wait for GPU
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        uint64_t fenceVal = 1;
        queue->Signal(fence.Get(), fenceVal);
        if (fence->GetCompletedValue() < fenceVal)
        {
            fence->SetEventOnCompletion(fenceVal, ev);
            WaitForSingleObject(ev, INFINITE);
        }
        CloseHandle(ev);

        // Map readback and compute prefix on CPU
        D3D12_RANGE readRange = {0, NumCounts * sizeof(uint32_t)};
        uint32_t *cbData = nullptr;
        blockCountsReadback->Map(0, &readRange, reinterpret_cast<void **>(&cbData));

        std::vector<uint32_t> cpuPrefix;
        ExclusivePrefixCPU(cbData, NumCounts, cpuPrefix);
        // compute totalZeros as sum of zeros across blocks (cpuPrefix[NumCounts-1] + last blockCounts value if needed)
        uint32_t totalZeros = cpuPrefix.empty() ? 0 : (cpuPrefix[NumCounts - 1] + cbData[NumCounts - 1]);

        blockCountsReadback->Unmap(0, nullptr);

        // Upload prefix to GPU (prefixUpload is an upload resource created in Init)
        // Map and copy cpuPrefix into prefixUpload
        uint32_t *prefixMapped = nullptr;
        prefixUpload->Map(0, &r, reinterpret_cast<void **>(&prefixMapped));
        memcpy(prefixMapped, cpuPrefix.data(), NumCounts * sizeof(uint32_t));
        prefixUpload->Unmap(0, nullptr);

        // Copy upload -> GPU prefixBuffer
        // Need a new command list for this copy and subsequent scatter dispatch
        alloc->Reset();
        cmdList->Reset(alloc.Get(), nullptr);
        cmdList->CopyResource(prefixBuffer.Get(), prefixUpload.Get());

        // Update params totalZeros and set root constants
        p.totalZeros = totalZeros;
        cmdList->SetComputeRoot32BitConstants(1, 4, &p, 0);

        // Scatter dispatch
        cmdList->SetPipelineState(m_scatterPSO.Get());
        cmdList->SetComputeRootSignature(m_csRootSig.Get());
        cmdList->SetComputeRootDescriptorTable(0, m_uavHeap->GetGPUDescriptorHandleForHeapStart());
        cmdList->Dispatch(NumBlocks, 1, 1);

        // UAV barrier to ensure Output written
        D3D12_RESOURCE_BARRIER uavBarrier2 = {};
        uavBarrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier2.UAV.pResource = sortBuffers[0].Get();
        cmdList->ResourceBarrier(1, &uavBarrier2);

        cmdList->Close();
        ID3D12CommandList *lists2[] = {cmdList.Get()};
        queue->ExecuteCommandLists(1, lists2);

        // Wait again for completion
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fenceVal++;
        queue->Signal(fence.Get(), fenceVal);
        if (fence->GetCompletedValue() < fenceVal)
        {
            fence->SetEventOnCompletion(fenceVal, ev);
            WaitForSingleObject(ev, INFINITE);
        }
        CloseHandle(ev);

        // TODO: ???
        // swap input/output pointers (we used sortBuffers[0] as input and [1] as output - but shader expects Input->Output as bound via UAVs in heap order)
        // For simplicity, we won't swap heap slots here; assume heap contains Input then Output and we swap resources behind them
        std::swap(sortBuffers[0], sortBuffers[1]);
    }
}