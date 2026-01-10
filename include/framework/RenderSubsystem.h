#pragma once

#include "pch.h"

#include "GPUSorting/GPUSorting.h"

#include "Camera.h"
#include "Cube.h"
#include "InputHandler.h"
#include "DescriptorAllocator.h"

struct GlobalConstants
{
	Matrix view;
	Matrix invView;
	Matrix proj;
	float nearPlane;
	float farPlane;
	float particleRadius;
	float _pad;
};

class RenderSubsystem
{
public:
	RenderSubsystem() = delete;
	static void Init();
	static void Destroy();
	static void Draw();

	// Access the main command queue for uploads/compute dispatches
	static ID3D12CommandQueue *GetCommandQueue();

	// TODO: move somewhere else
	// Wait helper: signals the provided fence with the given value and waits for completion
	static void WaitForFence(ID3D12Fence *fence, uint64_t fenceValue);
	static GPUSorting::DeviceInfo GetDeviceInfo() { return g_deviceInfo; };
	static std::shared_ptr<DescriptorAllocator> GetCBVSRVUAVAllocatorGPUVisible() { return m_cbvSrvUavAllocatorGPUVisible; };
	static std::shared_ptr<DescriptorAllocator> GetCBVSRVUAVAllocatorCPU() { return m_cbvSrvUavAllocatorCPU; };
	static winrt::com_ptr<ID3D12Device> GetDevice();

private:
	static void CreateDevice();
	static void CreateCommandQueueAndList();
	static void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);

	static void CreateFence();
	static void GetDescriptorSizes();
	static UINT Check4xMSAAQualitySupport();
	static HWND CreateMainWindow(HINSTANCE hInstance, int width, int height, LPCWSTR windowName);
	static void CreateDescriptorHeaps();
	static void CreateRenderTargetViews();
	static void CreateDepthStencil();

	static GPUSorting::DeviceInfo GetDeviceInfo(ID3D12Device *device);

	inline static winrt::com_ptr<ID3D12Device> m_device = nullptr;
	inline static winrt::com_ptr<ID3D12CommandQueue> m_commandQueue = nullptr;
	inline static winrt::com_ptr<IDXGISwapChain3> m_swapChain = nullptr;

	inline static GPUSorting::DeviceInfo g_deviceInfo = {};

	// TODO: replace with descriptor allocator
	inline static winrt::com_ptr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
	inline static winrt::com_ptr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;

	// cpu write only
	inline static std::shared_ptr<DescriptorAllocator> m_cbvSrvUavAllocatorGPUVisible = nullptr;
	// CPU read write
	inline static std::shared_ptr<DescriptorAllocator> m_cbvSrvUavAllocatorCPU = nullptr;

	inline static winrt::com_ptr<ID3D12CommandAllocator> m_commandAllocator = nullptr;
	inline static winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList = nullptr;

	inline static HWND m_windowHandle = nullptr;

	inline static constexpr uint32_t FrameCount = 2;

	inline static winrt::com_ptr<ID3D12Fence> m_fence;
	inline static uint64_t m_fenceValue = 0;
	inline static uint32_t m_rtvDescriptorSize = 0;
	inline static uint32_t m_dsvDescriptorSize = 0;
	inline static uint32_t m_cbvSrvUavDescriptorSize = 0;

	// Render targets and depth stencil
	inline static winrt::com_ptr<ID3D12Resource> m_renderTargets[FrameCount];
	inline static winrt::com_ptr<ID3D12Resource> m_depthStencil;
	inline static D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHeapStart{};
	inline static D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHeapStart{};

	inline static uint32_t m_width = 0;
	inline static uint32_t m_height = 0;

	inline static winrt::com_ptr<ID3D12Resource> m_vertexBuffer;
	inline static D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
	inline static winrt::com_ptr<ID3D12PipelineState> m_particlePipelineState;
	inline static winrt::com_ptr<ID3D12PipelineState> m_cubePipelineState;
	inline static winrt::com_ptr<ID3D12RootSignature> m_rootSignature;

	// --- Global constant buffer ---
	inline static winrt::com_ptr<ID3D12Resource> m_globalConstantBuffer;
	inline static D3D12_GPU_VIRTUAL_ADDRESS m_globalCBAddress;
	inline static D3D12_CPU_DESCRIPTOR_HANDLE m_globalCBV{};
	inline static GlobalConstants m_globalConstants;

	inline static void CreateGlobalConstantBuffer();
	inline static void UpdateGlobalConstantBuffer();

	inline static void CreateRootSignatureAndPipeline();

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Camera / input state
	// timing
	inline static std::chrono::steady_clock::time_point m_lastFrameTime;

	// Input handler will update the camera
	inline static InputHandler m_inputHandler;

	inline static Cube cube1;
	inline static Cube cube2;
	inline static Camera camera;
};