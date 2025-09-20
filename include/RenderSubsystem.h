#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>

class RenderSubsystem
{
public:

	void Initialize();
	void Shutdown();
	void Draw();

private:
	void CreateDevice();
	void CreateCommandQueueAndList();
	void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
	
	void CreateFence();
	void GetDescriptorSizes();
	UINT Check4xMSAAQualitySupport();
	HWND CreateMainWindow(HINSTANCE hInstance, int width, int height, LPCWSTR windowName);
	void CreateDescriptorHeaps();
	void CreateRenderTargetViews();
	void CreateDepthStencil();

	void CreateTriangleResources();
	void RenderTriangle();

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	HWND m_windowHandle = nullptr;

	static constexpr uint32_t FrameCount = 2;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValue = 0;
	uint32_t m_rtvDescriptorSize = 0;
	uint32_t m_dsvDescriptorSize = 0;
	uint32_t m_cbvSrvUavDescriptorSize = 0;

	// Render targets and depth stencil
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHeapStart{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHeapStart{};

	uint32_t m_width = 0;
	uint32_t m_height = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
};