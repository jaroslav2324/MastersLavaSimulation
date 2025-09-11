#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi.h>
#include <cstdint>

class RenderSubsystem
{
public:
	RenderSubsystem();
	RenderSubsystem(HWND hwnd, uint32_t width, uint32_t height);
	~RenderSubsystem();

	void Initialize();
	void Present();

private:
	void CreateDevice();
	void CreateCommandQueue();
	void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
	void CreateRTVHeap();

	void CreateFence();
	void GetDescriptorSizes();
	UINT Check4xMSAAQualitySupport();
	void CreateCommandQueueAndList();

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	static constexpr uint32_t FrameCount = 2;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValue = 0;
	uint32_t m_rtvDescriptorSize = 0;
	uint32_t m_dsvDescriptorSize = 0;
	uint32_t m_cbvSrvUavDescriptorSize = 0;
};