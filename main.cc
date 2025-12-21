
#include "pch.h"

#include "framework/RenderSubsystem.h"
#include "framework/SimulationSystem.h"

int main(int argc, char **argv)
{
	RenderSubsystem::Init();
	SimulationSystem::Init(RenderSubsystem::GetDevice().get());

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
			// TODO: pass delta time
			SimulationSystem::Simulate(1.0f / 60.0f);
			RenderSubsystem::Draw();
		}
	}

	RenderSubsystem::Destroy();
	return 0;
}