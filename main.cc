
#include "pch.h"

#include <fstream>
#include <iostream>

#include "framework/Time.h"
#include "framework/RenderSubsystem.h"
#include "framework/SimulationSystem.h"

int main(int argc, char **argv)
{
	UINT particleCount = 0;
	if (argc > 1)
	{
		try
		{
			unsigned long parsed = std::stoul(argv[1]);
			if (parsed == 0)
			{
				std::cout << "Usage: " << argv[0] << " <numParticles>\n";
				return 1;
			}
			particleCount = static_cast<UINT>(parsed);
		}
		catch (...)
		{
			std::cout << "Usage: " << argv[0] << " <numParticles>\n";
			return 1;
		}
	}

	RenderSubsystem::Init();
	if (particleCount > 0)
	{
		SimulationSystem::SetMaxParticlesCount(particleCount);
	}
	SimulationSystem::Init(RenderSubsystem::GetDevice().get());
	if (particleCount == 0)
	{
		particleCount = SimulationSystem::GetNumParticles();
	}

	std::cout << "Simulation particle count: " << particleCount << "\n";
	SimulationSystem::StartSimulation();

	std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();

	TimeAccumulator simTimeAcc;
	TimeAccumulator renderTimeAcc;

	const int stopAtSimFrames = 10000;	  // 0 = disabled
	const double stopAtSimSeconds = 90.0; // 0.0 = disabled

	double elapsedSimSeconds = 0.0;

	MSG msg = {};
	while (msg.message != WM_QUIT &&
		   (stopAtSimFrames == 0 || simTimeAcc.count() < (size_t)stopAtSimFrames) &&
		   (stopAtSimSeconds <= 0.0 || elapsedSimSeconds < stopAtSimSeconds))
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
			if (simulationEnabled)
			{
				elapsedSimSeconds += deltaTime.count();
			}

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
	std::cout << "Elapsed sim time: " << elapsedSimSeconds << " s\n";
	std::cout << "Simulation avg  : " << simAvg << " ms\n";
	std::cout << "Render avg      : " << renAvg << " ms\n";
	std::cout << "Total avg       : " << total << " ms\n";
	std::cout << "==========================\n";

	const std::string resultsFileName = "simulation_results.csv";
	std::ofstream resultsFile(resultsFileName, std::ios::app);
	if (resultsFile)
	{
		if (resultsFile.tellp() == 0)
		{
			resultsFile << "particles,sim_ms,render_ms,total_ms\n";
		}
		resultsFile << particleCount << ',' << simAvg << ',' << renAvg << ',' << total << "\n";
		std::cout << "Results appended to " << resultsFileName << "\n";
	}
	else
	{
		std::cout << "Failed to open " << resultsFileName << " for writing.\n";
	}

	RenderSubsystem::Destroy();
	return 0;
}