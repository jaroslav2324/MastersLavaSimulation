
#include "pch.h"

#include "framework/Time.h"
#include "framework/RenderSubsystem.h"
#include "framework/SimulationSystem.h"

int main(int argc, char **argv)
{
	RenderSubsystem::Init();
	SimulationSystem::Init(RenderSubsystem::GetDevice().get());

	std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();

	TimeAccumulator simTimeAcc;
	TimeAccumulator renderTimeAcc;

	const int stopAtSimFrames = 10000;

	MSG msg = {};
	while (msg.message != WM_QUIT && (stopAtSimFrames == 0 || simTimeAcc.count() < stopAtSimFrames))
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

			bool simulationEnabled = SimulationSystem::IsRunning();

			{
				ConditionalScopedTimer simTimer(simulationEnabled ? &simTimeAcc : nullptr);
				SimulationSystem::Simulate(deltaTime.count());
			}

			{
				ConditionalScopedTimer renderTimer(simulationEnabled ? &renderTimeAcc : nullptr);
				RenderSubsystem::Draw();
			}
		}
	}

	double simAvg = simTimeAcc.average();
	double renAvg = renderTimeAcc.average();
	double total = simAvg + renAvg;

	std::cout << "\n=== Frame Timing Stats ===\n";
	std::cout << "Frames measured : " << simTimeAcc.count() << "\n";
	std::cout << "Simulation avg  : " << simAvg << " ms\n";
	std::cout << "Render avg      : " << renAvg << " ms\n";
	std::cout << "Total avg       : " << total << " ms\n";
	std::cout << "==========================\n";

	RenderSubsystem::Destroy();
	return 0;
}