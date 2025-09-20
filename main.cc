#include <iostream>

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment (lib,"D3D12.lib")
#pragma comment(lib,"dxgi.lib")

#include <vector>

#include "RenderSubsystem.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi.h>
#include <cstdint>

int main(int argc, char** argv)
{
	RenderSubsystem renderer;
	renderer.Initialize();

	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			renderer.Draw();
		}
	}

	renderer.Shutdown();

	return 0;
}