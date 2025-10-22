#include "RenderSubsystem.h"
#include "ShaderCompiler.h"

#include <stdexcept>

// TODO: include normally
#include "../external/DirectXTK12/Inc/SimpleMath.h"

void RenderSubsystem::Initialize()
{
    m_width = 800;
    m_height = 600;
    m_windowHandle = CreateMainWindow(GetModuleHandle(nullptr), (int)m_width, (int)m_height, L"DX12 Window");
    CreateDevice();
    CreateFence();
    GetDescriptorSizes();
    CreateCommandQueueAndList();
    CreateSwapChain(m_windowHandle, m_width, m_height);
    CreateDescriptorHeaps();
    CreateRenderTargetViews();
    CreateDepthStencil();
    CreateTriangleResources();
}

void RenderSubsystem::CreateDevice()
{
#if defined(DEBUG) || defined(_DEBUG)

    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            throw std::runtime_error("Failed to get D3D12 debug interface.");
        }
        debugController->EnableDebugLayer();
    }
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DXGI Factory.");

    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &hardwareAdapter))
        {
            break; // No more adapters to enumerate.
        }

        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Try to create device
        hr = D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0, // TODO: replace to 12_0?
            IID_PPV_ARGS(&m_device));

        if (SUCCEEDED(hr))
            break;
    }

    // Fallback to WARP device if no hardware adapter found
    if (!m_device)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
        hr = D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create D3D12 device.");
    }

    if (Check4xMSAAQualitySupport() < 1)
        throw std::runtime_error("Device does not support 4x MSAA.");
}

void RenderSubsystem::CreateFence()
{
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create fence.");
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
    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command queue.");

    // Create command allocator
    hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command allocator.");

    // Create command list
    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(),
        nullptr, // TODO : Initial Pipeline State?
        IID_PPV_ARGS(&m_commandList));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create command list.");

    // Command lists are created in the recording state. Close it for now.
    m_commandList->Close();
}

void RenderSubsystem::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
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
    swapChainDesc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create swap chain.");

    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr))
        throw std::runtime_error("Failed to query IDXGISwapChain3.");
}

HWND RenderSubsystem::CreateMainWindow(HINSTANCE hInstance, int width, int height, LPCWSTR windowName)
{
    // Define window class
    WNDCLASSW wc = {};               // Use WNDCLASSW for wide strings
    wc.lpfnWndProc = DefWindowProcW; // Use DefWindowProcW for wide strings
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
    // RTV Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount; // One RTV per swap chain buffer
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create RTV descriptor heap.");

    // DSV Heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1; // Usually one DSV
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DSV descriptor heap.");

    // CBV/SRV/UAV Heap (shader-visible)
    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
    cbvSrvUavHeapDesc.NumDescriptors = 64; // Adjust as needed for your resources
    cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = m_device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create CBV/SRV/UAV descriptor heap.");

    m_rtvHeapStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_dsvHeapStart = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void RenderSubsystem::CreateRenderTargetViews()
{
    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        if (FAILED(hr))
            throw std::runtime_error("Failed to get swap chain buffer.");

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeapStart;
        rtvHandle.ptr += size_t(i) * m_rtvDescriptorSize;

        // Create RTV for this back buffer
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
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

    // Use explicit heap properties so d3dx12.h is not required
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
        IID_PPV_ARGS(&m_depthStencil));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create depth stencil resource.");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeapStart);
}

void RenderSubsystem::Shutdown()
{
    // Ensure GPU is done before releasing resources
    if (m_commandQueue && m_fence)
    {
        m_fenceValue++;
        m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
        if (m_fence->GetCompletedValue() < m_fenceValue)
        {
            HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            m_fence->SetEventOnCompletion(m_fenceValue, eventHandle);
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }
    // Release resources in reverse order of creation
    m_depthStencil.Reset();
    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        m_renderTargets[i].Reset();
    }
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_cbvSrvUavHeap.Reset();
    m_swapChain.Reset();
    m_commandList.Reset();
    m_commandAllocator.Reset();
    m_commandQueue.Reset();
    m_fence.Reset();
    m_device.Reset();
    if (m_windowHandle)
    {
        DestroyWindow(m_windowHandle);
        m_windowHandle = nullptr;
    }
}

void RenderSubsystem::Draw()
{
    // Wait for previous frame to finish
    if (m_commandQueue && m_fence)
    {
        m_fenceValue++;
        m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
        if (m_fence->GetCompletedValue() < m_fenceValue)
        {
            HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            m_fence->SetEventOnCompletion(m_fenceValue, eventHandle);
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    RenderTriangle();
}

void RenderSubsystem::CreateTriangleResources()
{
    // Define a local vertex struct using Vector3 for position and color
    struct TriangleVertex
    {
        DirectX::SimpleMath::Vector3 position;
        DirectX::SimpleMath::Vector3 color;
    };

    // Vertex buffer for triangle
    TriangleVertex triangleVertices[] =
        {
            {DirectX::SimpleMath::Vector3(0.0f, 0.5f, 0.0f), DirectX::SimpleMath::Vector3(1.0f, 0.0f, 0.0f)},
            {DirectX::SimpleMath::Vector3(0.5f, -0.5f, 0.0f), DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f)},
            {DirectX::SimpleMath::Vector3(-0.5f, -0.5f, 0.0f), DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f)},
        };

    // Create vertex buffer
    {
        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Explicit heap properties
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        // Explicit resource desc
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = vertexBufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create triangle vertex buffer.");

        // Copy the triangle data to the buffer
        UINT8 *pVertexData;
        D3D12_RANGE readRange = {0, 0};
        hr = m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&pVertexData));
        if (FAILED(hr))
            throw std::runtime_error("Failed to map triangle vertex buffer.");

        memcpy(pVertexData, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);
    }

    // Create root signature
    {
        D3D12_ROOT_PARAMETER rootParameters[1] = {};
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = 1;
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr))
        {
            if (error)
                OutputDebugStringA((char *)error->GetBufferPointer());
            throw std::runtime_error("Failed to serialize root signature.");
        }

        hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create root signature.");
    }

    // Compile shaders
    auto vsResult = ShaderCompiler::CompileFromFile(
        L"shaders/TriangleVS.hlsl", "main", "vs_5_0");
    if (!vsResult.success)
        throw std::runtime_error("Vertex shader compilation failed.");

    auto psResult = ShaderCompiler::CompileFromFile(
        L"shaders/TrianglePS.hlsl", "main", "ps_5_0");
    if (!psResult.success)
        throw std::runtime_error("Pixel shader compilation failed.");

    // Create PSO (Pipeline State Object)
    {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
            {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TriangleVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TriangleVertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = {inputLayout, _countof(inputLayout)};
        psoDesc.pRootSignature = m_rootSignature.Get();

        psoDesc.VS.pShaderBytecode = vsResult.bytecode->GetBufferPointer();
        psoDesc.VS.BytecodeLength = vsResult.bytecode->GetBufferSize();
        psoDesc.PS.pShaderBytecode = psResult.bytecode->GetBufferPointer();
        psoDesc.PS.BytecodeLength = psResult.bytecode->GetBufferSize();

        // Rasterizer, blend, depth-stencil
        psoDesc.RasterizerState = {};
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        psoDesc.BlendState = {};
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL};
        for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            psoDesc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;

        psoDesc.DepthStencilState = {};
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create graphics pipeline state.");
    }
}

void RenderSubsystem::RenderTriangle()
{
    // Reset command allocator and command list
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());

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
    barrier.Transition.pResource = m_renderTargets[backBufferIndex].Get();
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

    // Set pipeline state and root signature
    m_commandList->SetPipelineState(m_pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Set primitive topology
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set vertex buffer
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes = static_cast<UINT>(3 * sizeof(DirectX::SimpleMath::Vector3) * 2); // 3 vertices, 2 Vector3 per vertex
    vbView.StrideInBytes = sizeof(DirectX::SimpleMath::Vector3) * 2;
    m_commandList->IASetVertexBuffers(0, 1, &vbView);

    // Draw triangle
    m_commandList->DrawInstanced(3, 1, 0, 0);

    // Transition back buffer to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    // Close and execute command list
    m_commandList->Close();
    ID3D12CommandList *ppCommandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // Present
    m_swapChain->Present(1, 0);

    // Wait for GPU (simple sync)
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        m_fence->SetEventOnCompletion(m_fenceValue, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}
