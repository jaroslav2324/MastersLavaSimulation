#include "framework/RenderSubsystem.h"

#include "framework/ShaderCompiler.h"
#include "framework/RenderTemplatesAPI.h"
#include "framework/SimulationSystem.h"

ID3D12CommandQueue *RenderSubsystem::GetCommandQueue()
{
    return m_commandQueue.get();
}

winrt::com_ptr<ID3D12Device> RenderSubsystem::GetDevice()
{
    return m_device;
}

void RenderSubsystem::Init()
{
    m_width = 800;
    m_height = 600;
    m_windowHandle = CreateMainWindow(GetModuleHandle(nullptr), (int)m_width, (int)m_height, L"DX12 Window");

    CreateDevice();
    g_deviceInfo = GetDeviceInfo(m_device.get());

    CreateFence();
    GetDescriptorSizes();
    CreateCommandQueueAndList();
    CreateSwapChain(m_windowHandle, m_width, m_height);
    CreateDescriptorHeaps();
    CreateRenderTargetViews();
    CreateDepthStencil();

    // initialize timing and camera
    m_lastFrameTime = std::chrono::steady_clock::now();
    camera = Camera();

    cube1.Initialize(m_device.get(), m_commandList.get());
    cube2.Initialize(m_device.get(), m_commandList.get());

    cube1.GetTransform().position = Vector3(-1.0f, 0.0f, -5.0f);
    cube2.GetTransform().position = Vector3(2.0f, 0.0f, -5.0f);

    CreateGlobalConstantBuffer();

    cube1.CreateConstBuffer(m_device.get(), *m_cbvSrvUavAllocator, 1);
    cube2.CreateConstBuffer(m_device.get(), *m_cbvSrvUavAllocator, 2);

    CreateRootSignatureAndPipeline();

    m_commandList->Close();
    ID3D12CommandList *ppCommandLists[] = {m_commandList.get()};
    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
}

void RenderSubsystem::CreateGlobalConstantBuffer()
{
    // TODO: replace with helper function
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // TODO: replace with helper function
    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Alignment = 0;
    cbDesc.Width = (sizeof(GlobalConstants) + 255) & ~255; // CB size must be 256-byte aligned
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.SampleDesc.Quality = 0;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_globalConstantBuffer.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create global constant buffer.");

    m_globalCBAddress = m_globalConstantBuffer->GetGPUVirtualAddress();

    // Create CBV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_globalCBAddress;
    cbvDesc.SizeInBytes = (sizeof(GlobalConstants) + 255) & ~255;
    // allocate a slot in the CBV/SRV/UAV allocator and write CBV there
    UINT cbIndex = m_cbvSrvUavAllocator->Alloc();
    m_globalCBV = m_cbvSrvUavAllocator->GetCpuHandle(cbIndex);
    m_device->CreateConstantBufferView(&cbvDesc, m_globalCBV);
}

void RenderSubsystem::UpdateGlobalConstantBuffer()
{
    // TODO: get all from camera
    // Use Camera class to build view matrix
    Matrix view = camera.GetViewMatrix();
    m_globalConstants.view = view;             //.Transpose();
    m_globalConstants.invView = view.Invert(); //.Transpose();
    m_globalConstants.proj = Matrix::CreatePerspectiveFieldOfView(
        DirectX::XMConvertToRadians(60.0f), float(m_width) / float(m_height), 0.1f, 100.0f); //.Transpose();
    m_globalConstants.nearPlane = 0.1f;
    m_globalConstants.farPlane = 100.0f;

    m_globalConstants.particleRadius = 0.8 * SimulationSystem::GetKernelRadius() / 2.0;

    // Map and copy data
    UINT8 *pData;
    D3D12_RANGE readRange = {0, 0};
    HRESULT hr = m_globalConstantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&pData));
    if (FAILED(hr))
        throw std::runtime_error("Failed to map global constant buffer.");
    memcpy(pData, &m_globalConstants, sizeof(GlobalConstants));
    m_globalConstantBuffer->Unmap(0, nullptr);
}

void RenderSubsystem::CreateDevice()
{
#if defined(DEBUG) || defined(_DEBUG)

    {
        winrt::com_ptr<ID3D12Debug> debugController;
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put()))))
        {
            throw std::runtime_error("Failed to get D3D12 debug interface.");
        }
        debugController->EnableDebugLayer();
    }
#endif

    winrt::com_ptr<IDXGIFactory6> factory;
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(factory.put())));

    winrt::com_ptr<IDXGIAdapter1> bestAdapter;
    SIZE_T bestVideoMemory = 0;

    for (UINT i = 0;; ++i)
    {
        winrt::com_ptr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapterByGpuPreference(
                i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.put())) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Проверяем, что D3D12 реально поддерживается
        if (FAILED(D3D12CreateDevice(
                adapter.get(),
                D3D_FEATURE_LEVEL_11_0,
                __uuidof(ID3D12Device),
                nullptr)))
        {
            continue;
        }

        if (desc.DedicatedVideoMemory > bestVideoMemory)
        {
            bestVideoMemory = desc.DedicatedVideoMemory;
            bestAdapter = adapter;
        }
    }

    if (!bestAdapter)
        throw std::runtime_error("No suitable hardware adapter found.");

    ThrowIfFailed(D3D12CreateDevice(
        bestAdapter.get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(m_device.put())));
}

void RenderSubsystem::CreateFence()
{
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create fence.");
}

void RenderSubsystem::WaitForFence(ID3D12Fence *fence, uint64_t fenceValue)
{
    auto queue = GetCommandQueue();
    queue->Signal(fence, fenceValue);

    if (fence->GetCompletedValue() < fenceValue)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(fenceValue, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void RenderSubsystem::GetDescriptorSizes()
{
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

UINT RenderSubsystem::Check4xMSAAQualitySupport()
{
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels = {};
    msaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    msaaQualityLevels.SampleCount = 4;
    msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msaaQualityLevels.NumQualityLevels = 0;

    HRESULT hr = m_device->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msaaQualityLevels,
        sizeof(msaaQualityLevels));
    if (FAILED(hr))
        throw std::runtime_error("Failed to check MSAA quality support.");

    return msaaQualityLevels.NumQualityLevels;
}

void RenderSubsystem::CreateCommandQueueAndList()
{
    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command queue.");

    // Create command allocator
    hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(m_commandAllocator.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command allocator.");

    // Create command list
    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.get(),
        nullptr, // TODO : Initial Pipeline State?
        IID_PPV_ARGS(m_commandList.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command list.");

    // Command lists are created in the recording state. Close it for now.
    // m_commandList->Close();
}

void RenderSubsystem::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
    winrt::com_ptr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(factory.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DXGI Factory for swap chain.");

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1; // No MSAA for swap chain
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Disable VSync

    IDXGISwapChain1 *rawSwapChainPtr = nullptr;

    hr = factory->CreateSwapChainForHwnd(m_commandQueue.get(), hwnd, &swapChainDesc, nullptr, nullptr, &rawSwapChainPtr);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create swap chain.");

    winrt::com_ptr<IDXGISwapChain1> swapChain1(rawSwapChainPtr, winrt::take_ownership_from_abi_t());
    hr = swapChain1->QueryInterface(IID_PPV_ARGS(m_swapChain.put()));
    if (FAILED(hr) || !m_swapChain)
        throw std::runtime_error("Failed to query IDXGISwapChain3");
}

HWND RenderSubsystem::CreateMainWindow(HINSTANCE hInstance, int width, int height, LPCWSTR windowName)
{
    // Define window class
    WNDCLASSW wc = {};              // Use WNDCLASSW for wide strings
    wc.lpfnWndProc = StaticWndProc; // NEW: custom WndProc
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DX12WindowClass";
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);

    // Register window class
    if (!RegisterClassW(&wc)) // Use RegisterClassW for wide strings
        throw std::runtime_error("Failed to register window class.");

    // Create window
    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        windowName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
        throw std::runtime_error("Failed to create window.");

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return hwnd;
}

void RenderSubsystem::CreateDescriptorHeaps()
{
    // TODO: replace with Allocator
    // RTV Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount; // One RTV per swap chain buffer
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create RTV descriptor heap.");

    // TODO: replace with Allocator
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1; // Usually one DSV
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DSV descriptor heap.");

    m_cbvSrvUavAllocator = std::make_shared<DescriptorAllocator>();
    m_cbvSrvUavAllocator->Init(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, true);

    m_rtvHeapStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_dsvHeapStart = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void RenderSubsystem::CreateRenderTargetViews()
{
    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(m_renderTargets[i].put()));
        if (FAILED(hr))
            throw std::runtime_error("Failed to get swap chain buffer.");

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeapStart;
        rtvHandle.ptr += size_t(i) * m_rtvDescriptorSize;

        // Create RTV for this back buffer
        m_device->CreateRenderTargetView(m_renderTargets[i].get(), nullptr, rtvHandle);
    }
}

void RenderSubsystem::CreateDepthStencil()
{
    // Describe the depth/stencil buffer
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Alignment = 0;
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    // TODO: replace with helper function
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(m_depthStencil.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create depth stencil resource.");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    m_device->CreateDepthStencilView(m_depthStencil.get(), &dsvDesc, m_dsvHeapStart);
}

void RenderSubsystem::Destroy()
{
    // Ensure GPU is done before releasing resources
    if (m_commandQueue && m_fence)
    {
        m_fenceValue++;
        m_commandQueue->Signal(m_fence.get(), m_fenceValue);
        if (m_fence->GetCompletedValue() < m_fenceValue)
        {
            HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            m_fence->SetEventOnCompletion(m_fenceValue, eventHandle);
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    if (m_windowHandle)
    {
        DestroyWindow(m_windowHandle);
        m_windowHandle = nullptr;
    }
}

void RenderSubsystem::Draw()
{
    auto now = std::chrono::steady_clock::now();
    float deltaSeconds = std::chrono::duration<float>(now - m_lastFrameTime).count();
    if (deltaSeconds > 0.1f)
        deltaSeconds = 0.1f; // clamp large deltas
    m_lastFrameTime = now;

    m_inputHandler.ProcessInput(deltaSeconds, camera);

    // Wait for previous frame to finish
    if (m_commandQueue && m_fence)
    {
        m_fenceValue++;
        m_commandQueue->Signal(m_fence.get(), m_fenceValue);
        if (m_fence->GetCompletedValue() < m_fenceValue)
        {
            HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            m_fence->SetEventOnCompletion(m_fenceValue, eventHandle);
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    m_commandAllocator->Reset();
    // Initialize command list with cube pipeline (any valid PSO works)
    m_commandList->Reset(m_commandAllocator.get(), m_cubePipelineState.get());

    // Set viewport and scissor
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = {0, 0, (LONG)m_width, (LONG)m_height};
    m_commandList->RSSetScissorRects(1, &scissorRect);

    // Transition back buffer to render target
    UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[backBufferIndex].get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Set render target and depth stencil
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeapStart;
    rtvHandle.ptr += backBufferIndex * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHeapStart);

    // Clear render target and depth stencil
    const float clearColor[] = {0.1f, 0.1f, 0.3f, 1.0f};
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsvHeapStart, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set root signature and update global constants/descriptor heaps
    m_commandList->SetGraphicsRootSignature(m_rootSignature.get());

    UpdateGlobalConstantBuffer();
    ID3D12DescriptorHeap *heaps[] = {m_cbvSrvUavAllocator->GetHeap()};
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvSrvUavAllocator->GetHeap()->GetGPUDescriptorHandleForHeapStart());

    // --- Draw cubes ---
    // Use cube pipeline (vertex/index draw with POSITION float4 input)
    m_commandList->SetPipelineState(m_cubePipelineState.get());
    // cube.Draw will set its per-object CBV (root param 1) and issue DrawIndexed
    cube1.Draw(m_commandList.get());
    cube2.Draw(m_commandList.get());

    // --- Draw particles ---
    // Use particle pipeline (VS generates quads via SV_VertexID/SV_InstanceID)
    m_commandList->SetPipelineState(m_particlePipelineState.get());
    // Bind particle SRVs (root params 2 and 3)
    D3D12_GPU_DESCRIPTOR_HANDLE posHandle = SimulationSystem::GetPositionBufferSRV();
    D3D12_GPU_DESCRIPTOR_HANDLE tempHandle = SimulationSystem::GetTemperatureBufferSRV();
    m_commandList->SetGraphicsRootDescriptorTable(2, posHandle);
    m_commandList->SetGraphicsRootDescriptorTable(3, tempHandle);

    // Draw quads (4 vertices per particle) instanced
    uint32_t particleCount = SimulationSystem::GetNumParticles();

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    // Vertex shader uses SV_VertexID/SV_InstanceID to generate quad, so draw 4 vertices per instance
    m_commandList->DrawInstanced(4, particleCount, 0, 0);

    // Transition back buffer to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();
    ID3D12CommandList *ppCommandLists[] = {m_commandList.get()};
    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

    m_swapChain->Present(1, 0);

    // Wait for GPU (simple sync)
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        m_fence->SetEventOnCompletion(m_fenceValue, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

// test pipeline
void RenderSubsystem::CreateRootSignatureAndPipeline()
{
    // Root signature: keep CBV tables at indices 0 and 1 for existing cube code,
    // and reserve SRV tables at indices 2 and 3 for particle position and temperature.
    D3D12_DESCRIPTOR_RANGE cbvRange0 = CreateDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    D3D12_DESCRIPTOR_RANGE cbvRange1 = CreateDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    D3D12_DESCRIPTOR_RANGE srvRangePos = CreateDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    D3D12_DESCRIPTOR_RANGE srvRangeTemp = CreateDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    D3D12_ROOT_PARAMETER rootParams[4] = {
        CreateDescriptorTableRootParam(&cbvRange0, 1, D3D12_SHADER_VISIBILITY_ALL),
        CreateDescriptorTableRootParam(&cbvRange1, 1, D3D12_SHADER_VISIBILITY_ALL),
        CreateDescriptorTableRootParam(&srvRangePos, 1, D3D12_SHADER_VISIBILITY_ALL),
        CreateDescriptorTableRootParam(&srvRangeTemp, 1, D3D12_SHADER_VISIBILITY_ALL)};

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParams);
    rootSignatureDesc.pParameters = rootParams;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    winrt::com_ptr<ID3DBlob> serializedRootSig;
    winrt::com_ptr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.put(), errorBlob.put());
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
        throw std::runtime_error("Failed to serialize root signature.");
    }

    hr = m_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create root signature.");

    // Compile rendering shaders
    auto vsResult = ShaderCompiler::CompileFromFile(
        L"c:\\Users\\jaro_\\source\\repos\\MastersLavaSimulation\\shaders\\rendering\\Vertex.hlsl",
        "VSMain",
        "vs_5_0");
    if (!vsResult.success)
    {
        if (vsResult.error)
            OutputDebugStringA((char *)vsResult.error->GetBufferPointer());
        throw std::runtime_error("Failed to compile vertex shader from file.");
    }

    auto psResult = ShaderCompiler::CompileFromFile(
        L"c:\\Users\\jaro_\\source\\repos\\MastersLavaSimulation\\shaders\\rendering\\Pixel.hlsl",
        "PSMain",
        "ps_5_0");
    if (!psResult.success)
    {
        if (psResult.error)
            OutputDebugStringA((char *)psResult.error->GetBufferPointer());
        throw std::runtime_error("Failed to compile pixel shader from file.");
    }

    // --- Create particle PSO (no input layout; shader uses SV_VertexID/SV_InstanceID) ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePsoDesc = {};
    particlePsoDesc.InputLayout = {nullptr, 0};
    particlePsoDesc.pRootSignature = m_rootSignature.get();
    particlePsoDesc.VS = {vsResult.bytecode->GetBufferPointer(), vsResult.bytecode->GetBufferSize()};
    particlePsoDesc.PS = {psResult.bytecode->GetBufferPointer(), psResult.bytecode->GetBufferSize()};
    particlePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    particlePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    particlePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    particlePsoDesc.SampleMask = UINT_MAX;
    particlePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    particlePsoDesc.NumRenderTargets = 1;
    particlePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    particlePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    particlePsoDesc.SampleDesc.Count = 1;

    hr = m_device->CreateGraphicsPipelineState(&particlePsoDesc, IID_PPV_ARGS(m_particlePipelineState.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create particle pipeline state.");

    // --- Create cube PSO (with input layout matching Cube vertex format) ---
    auto cubeVsResult = ShaderCompiler::CompileFromFile(
        L"c:\\Users\\jaro_\\source\\repos\\MastersLavaSimulation\\shaders\\CubeShaders.hlsl",
        "VSMain",
        "vs_5_0");
    if (!cubeVsResult.success)
    {
        if (cubeVsResult.error)
            OutputDebugStringA((char *)cubeVsResult.error->GetBufferPointer());
        throw std::runtime_error("Failed to compile cube vertex shader.");
    }

    auto cubePsResult = ShaderCompiler::CompileFromFile(
        L"c:\\Users\\jaro_\\source\\repos\\MastersLavaSimulation\\shaders\\CubeShaders.hlsl",
        "PSMain",
        "ps_5_0");
    if (!cubePsResult.success)
    {
        if (cubePsResult.error)
            OutputDebugStringA((char *)cubePsResult.error->GetBufferPointer());
        throw std::runtime_error("Failed to compile cube pixel shader.");
    }

    // Input layout: POSITION as float4
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC cubePsoDesc = {};
    cubePsoDesc.InputLayout = {inputElements, _countof(inputElements)};
    cubePsoDesc.pRootSignature = m_rootSignature.get();
    cubePsoDesc.VS = {cubeVsResult.bytecode->GetBufferPointer(), cubeVsResult.bytecode->GetBufferSize()};
    cubePsoDesc.PS = {cubePsResult.bytecode->GetBufferPointer(), cubePsResult.bytecode->GetBufferSize()};
    cubePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    cubePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    cubePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    cubePsoDesc.SampleMask = UINT_MAX;
    cubePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    cubePsoDesc.NumRenderTargets = 1;
    cubePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    cubePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    cubePsoDesc.SampleDesc.Count = 1;

    hr = m_device->CreateGraphicsPipelineState(&cubePsoDesc, IID_PPV_ARGS(m_cubePipelineState.put()));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create cube pipeline state.");
}

LRESULT CALLBACK RenderSubsystem::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return WndProc(hwnd, msg, wParam, lParam);
    // return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT RenderSubsystem::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        m_inputHandler.OnKeyDown(wParam);
        return 0;
    case WM_KEYUP:
        m_inputHandler.OnKeyUp(wParam);
        return 0;

    case WM_MOUSEMOVE:
    {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        m_inputHandler.OnMouseMove(x, y);
        return 0;
    }

    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        return 0;

    case WM_RBUTTONDOWN:
    {
        SetCapture(hwnd);
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        m_inputHandler.OnMouseDown(true, x, y);
        return 0;
    }
    case WM_RBUTTONUP:
        ReleaseCapture();
        m_inputHandler.OnMouseUp(true);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

GPUSorting::DeviceInfo RenderSubsystem::GetDeviceInfo(ID3D12Device *device)
{
    GPUSorting::DeviceInfo devInfo = {};
    auto adapterLuid = device->GetAdapterLuid();
    winrt::com_ptr<IDXGIFactory4> factory;
    winrt::check_hresult(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.put())));

    winrt::com_ptr<IDXGIAdapter1> adapter;
    winrt::check_hresult(factory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(adapter.put())));

    DXGI_ADAPTER_DESC1 adapterDesc{};
    winrt::check_hresult(adapter->GetDesc1(&adapterDesc));

    devInfo.Description = adapterDesc.Description;
    devInfo.deviceId = adapterDesc.DeviceId;
    devInfo.vendorId = adapterDesc.VendorId;
    devInfo.dedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
    devInfo.sharedSystemMemory = adapterDesc.SharedSystemMemory;

    bool isWarpDevice = ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == DXGI_ADAPTER_FLAG_SOFTWARE) ||
                        (_wcsicmp(adapterDesc.Description, L"Microsoft Basic Render Driver") == 0);

    D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_7};
    winrt::check_hresult(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model)));

    static wchar_t const *shaderModelName[] = {L"cs_6_0", L"cs_6_1", L"cs_6_2", L"cs_6_3",
                                               L"cs_6_4", L"cs_6_5", L"cs_6_6", L"cs_6_7"};
    uint32_t index = model.HighestShaderModel & 0xF;
    winrt::check_hresult((index >= _countof(shaderModelName) ? E_UNEXPECTED : S_OK));
    devInfo.SupportedShaderModel = shaderModelName[index];

    D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = {};
    winrt::check_hresult(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options4 = {};
    winrt::check_hresult(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options4, sizeof(options4)));

    // 16 bit types nice, but not required
    // MatchAny also uneccessary
    devInfo.SIMDWidth = options1.WaveLaneCountMin;
    devInfo.SIMDMaxWidth = options1.WaveLaneCountMax;
    devInfo.SIMDLaneCount = options1.TotalLaneCount;
    devInfo.SupportsWaveIntrinsics = options1.WaveOps;
    devInfo.Supports16BitTypes = options4.Native16BitShaderOpsSupported;
    devInfo.SupportsDeviceRadixSort = (devInfo.SIMDWidth >= 4 && devInfo.SupportsWaveIntrinsics &&
                                       model.HighestShaderModel >= D3D_SHADER_MODEL_6_0);
    devInfo.SupportsOneSweep = devInfo.SupportsDeviceRadixSort && !isWarpDevice;

#ifdef _DEBUG
    std::wcout << L"Device:                  " << devInfo.Description << L"\n";
    std::wcout << L"Supported Shader Model:  " << devInfo.SupportedShaderModel << L"\n";
    std::cout << "Min wave width:            " << devInfo.SIMDWidth << "\n";
    std::cout << "Max wave width:            " << devInfo.SIMDMaxWidth << "\n";
    std::cout << "Total lanes:               " << devInfo.SIMDLaneCount << "\n";
    std::cout << "Dedicated video memory:    " << devInfo.dedicatedVideoMemory << "\n";
    std::cout << "Shared system memory       " << devInfo.sharedSystemMemory << "\n";
    std::cout << "Supports Wave Intrinsics:  " << (devInfo.SupportsWaveIntrinsics ? "Yes" : "No") << "\n";
    std::cout << "Supports 16Bit Types:      " << (devInfo.Supports16BitTypes ? "Yes" : "No") << "\n";
    std::cout << "Supports DeviceRadixSort:  " << (devInfo.SupportsDeviceRadixSort ? "Yes" : "No") << "\n";
    std::cout << "Supports OneSweep:         " << (devInfo.SupportsOneSweep ? "Yes" : "No") << "\n\n";
#endif

    return devInfo;
}
