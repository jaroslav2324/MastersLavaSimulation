#pragma once

#include "inc.h"

#include "Camera.h"
#include "Cube.h"
#include "InputHandler.h"
#include <chrono>

using namespace DirectX::SimpleMath;

struct GlobalConstants
{
	Matrix view;
	Matrix proj;
	float nearPlane;
	float farPlane;
	float pad[2];
};

class RenderSubsystem
{
public:
	void Initialize();
	void Shutdown();
	void Draw();
	// Access the shared CBV/SRV/UAV descriptor heap (shader-visible)
	static ID3D12DescriptorHeap *GetCBVSRVUAVHeap();
	static UINT GetCBVSRVUAVDescriptorSize();
	// Access the main command queue for uploads/compute dispatches
	static ID3D12CommandQueue* GetCommandQueue();

private:
	// Global instance pointer for static accessors
	static RenderSubsystem* s_instance;
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

	// --- Global constant buffer ---
	Microsoft::WRL::ComPtr<ID3D12Resource> m_globalConstantBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS m_globalCBAddress;
	D3D12_CPU_DESCRIPTOR_HANDLE m_globalCBV{};
	GlobalConstants m_globalConstants;

	void CreateGlobalConstantBuffer();
	void UpdateGlobalConstantBuffer();

	// NEW: create root signature and pipeline state
	void CreateRootSignatureAndPipeline();

	// NEW: input & camera handling
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Camera / input state
	// timing
	std::chrono::steady_clock::time_point m_lastFrameTime;

	// Input handler will update the camera
	InputHandler m_inputHandler;

	Cube cube1;
	Cube cube2;
	Camera camera;
};