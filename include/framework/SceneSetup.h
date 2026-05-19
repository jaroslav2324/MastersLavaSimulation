#pragma once

#include "pch.h"

enum class SceneType
{
    UniformGrid,
    DenseRandom,
    DenseBottomWithSphere,
    DamBreak,
    TwoSpheres,
};

namespace SceneSetup
{
    /// @brief Fills outPositions and outTemperatures with initial particle data for the given scene.
    ///        Scene geometry is scaled proportionally to numParticles (see SceneFactor in .cc),
    ///        then clamped so all particles stay inside the simulation grid.
    /// @param scene        Which scene layout to generate.
    /// @param numParticles Number of particles to generate.
    /// @param outPositions Output: world-space particle positions.
    /// @param outTemperatures Output: per-particle temperatures (Kelvin).
    /// @param gridWorldSize Side length of the cubic simulation grid in world units.
    ///                      Pass SimulationSystem::GetGridWorldSize().
    ///                      0 means unconstrained (no clamping applied).
    void LoadScene(SceneType scene, UINT numParticles,
                   std::vector<DirectX::SimpleMath::Vector3> &outPositions,
                   std::vector<float> &outTemperatures,
                   float gridWorldSize = 0.0f);
}
