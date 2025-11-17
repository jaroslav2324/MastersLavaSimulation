#pragma once

#include "inc.h"

struct ParticleStateSwapBuffers
{
    Microsoft::WRL::ComPtr<ID3D12Resource> position[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> predictedPosition[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> velocity[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> temperature[2];
};

struct ParticleScratchBuffers
{
    Microsoft::WRL::ComPtr<ID3D12Resource> density;
    Microsoft::WRL::ComPtr<ID3D12Resource> lambda;
    Microsoft::WRL::ComPtr<ID3D12Resource> deltaP;
    Microsoft::WRL::ComPtr<ID3D12Resource> phase;
};