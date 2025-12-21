
#include "pch.h"

#include "framework/RenderSubsystem.h"

#include "GPUSorting/GPUSorting.h"
#include "GPUSorting/FFXParallelSort.h"
#include "GPUSorting/DeviceRadixSort.h"
#include "GPUSorting/OneSweep.h"

int main(int argc, char **argv)
{

	RenderSubsystem renderer;
	renderer.Initialize();

	winrt::com_ptr<ID3D12Device> device = RenderSubsystem::GetDevice();
	// GPUSorting::DeviceInfo deviceInfo = GetDeviceInfo(device.get());

	// DeviceRadixSort *drs = new DeviceRadixSort(
	// 	device,
	// 	deviceInfo,
	// 	GPUSorting::ORDER_ASCENDING,
	// 	GPUSorting::KEY_UINT32,
	// 	GPUSorting::PAYLOAD_UINT32);
	// drs->TestAll();
	// drs->BatchTiming(1 << 20, 100, 10, GPUSorting::ENTROPY_PRESET_1);
	// drs->~DeviceRadixSort();

	// OneSweep *oneSweep = new OneSweep(
	// 	device,
	// 	deviceInfo,
	// 	GPUSorting::ORDER_ASCENDING,
	// 	GPUSorting::KEY_UINT32,
	// 	GPUSorting::PAYLOAD_UINT32);
	// oneSweep->TestAll();
	// oneSweep->BatchTiming(1 << 20, 100, 10, GPUSorting::ENTROPY_PRESET_1);
	// oneSweep->~OneSweep();

	// FFXParallelSort *ffxPs = new FFXParallelSort(
	// 	device,
	// 	deviceInfo,
	// 	GPUSorting::ORDER_ASCENDING,
	// 	GPUSorting::KEY_UINT32,
	// 	GPUSorting::PAYLOAD_UINT32);
	// ffxPs->TestAll();
	// ffxPs->BatchTiming(1 << 20, 100, 10, GPUSorting::ENTROPY_PRESET_1);
	// ffxPs->~FFXParallelSort();

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			renderer.Draw();
		}
	}

	renderer.Shutdown();

	return 0;
}