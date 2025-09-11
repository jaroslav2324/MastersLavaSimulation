#include "RenderSubsystem.h"

void RenderSubsystem::Initialize()
{
	CreateDevice();
	CreateFence();
	GetDescriptorSizes();
}

void RenderSubsystem::CreateDevice()
{
#if	defined(DEBUG)	||	defined(_DEBUG)	

	{
		ComPtr<ID3D12Debug>	debugController;
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

	Microsoft::WRL::ComPtr<IDXGIAdapter> hardwareAdapter;
	for (UINT adapterIndex = 0; ; ++adapterIndex)
	{
		if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters(adapterIndex, &hardwareAdapter))
		{
			break; // No more adapters to enumerate.
		}

		DXGI_ADAPTER_DESC desc;
		hardwareAdapter->GetDesc(&desc);

		// Skip software adapters
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// Try to create device
		hr = D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0, // TODO: replace to 12_0?
			IID_PPV_ARGS(&m_device)
		);

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
			IID_PPV_ARGS(&m_device)
		);
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
		sizeof(msaaQualityLevels)
	);
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
		IID_PPV_ARGS(&m_commandAllocator)
	);
	if (FAILED(hr))
		throw std::runtime_error("Failed to create command allocator.");

	// Create command list
	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator.Get(),
		nullptr, // TODO : Initial Pipeline State?
		IID_PPV_ARGS(&m_commandList)
	);
	if (FAILED(hr))
		throw std::runtime_error("Failed to create command list.");

	// Command lists are created in the recording state. Close it for now.
	m_commandList->Close();
}