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
    // Fills outPositions and outTemperatures for the given scene.
    void LoadScene(SceneType scene, UINT numParticles,
                   std::vector<DirectX::SimpleMath::Vector3> &outPositions,
                   std::vector<float> &outTemperatures);
}
