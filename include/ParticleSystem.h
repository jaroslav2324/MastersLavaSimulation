#pragma once

#include <cmath>

#include "inc.h"
#include "Particle.h"

class ParticleSystem
{
public:
    ParticleSystem() = delete;
    static void Init(ID3D12Device *device);
    static void SwapParticleBuffers()
    {
        m_currentSwapIndex = (m_currentSwapIndex + 1) % 2;
    }

private:
    const static int m_maxParticlesCount = 1 << 20; // 1 million particles
    inline static unsigned int m_currentSwapIndex = 0;
    inline static ParticleStateSwapBuffers particleSwapBuffers{};
    inline static ParticleScratchBuffers particleScratchBuffers{};
};