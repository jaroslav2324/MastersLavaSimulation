#pragma once

#include "pch.h"

struct ParticleStateSwapBuffers
{
    winrt::com_ptr<ID3D12Resource> position[2];
    winrt::com_ptr<ID3D12Resource> predictedPosition[2];
    winrt::com_ptr<ID3D12Resource> velocity[2];
    winrt::com_ptr<ID3D12Resource> temperature[2];
};

struct ParticleScratchBuffers
{
    winrt::com_ptr<ID3D12Resource> density;
    winrt::com_ptr<ID3D12Resource> lambda;
    winrt::com_ptr<ID3D12Resource> deltaP;
    winrt::com_ptr<ID3D12Resource> phase;
};