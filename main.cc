
#include "pch.h"

#include <chrono>

#include "framework/RenderSubsystem.h"
#include "framework/SimulationSystem.h"

int main(int argc, char **argv)
{
	// std::cout << std::filesystem::current_path() << std::endl;

	RenderSubsystem::Init();
	SimulationSystem::Init(RenderSubsystem::GetDevice().get());

	std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();

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
			std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
			std::chrono::duration<float> deltaTime = currentTime - lastTime;
			lastTime = currentTime;

			SimulationSystem::Simulate(deltaTime.count());
			RenderSubsystem::Draw();
		}
	}

	RenderSubsystem::Destroy();
	return 0;
}