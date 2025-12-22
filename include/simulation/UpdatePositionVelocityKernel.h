#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

namespace SimulationKernels
{
    class UpdatePositionVelocity : public SimulationComputeKernelBase
    {
    public:
        UpdatePositionVelocity(
            winrt::com_ptr<ID3D12Device> device,
            const GPUSorting::DeviceInfo &info,
            const std::vector<std::wstring> &compileArguments,
            const std::filesystem::path &shaderPath,
            winrt::com_ptr<ID3D12RootSignature> rootSignature) : SimulationComputeKernelBase(device,
                                                                                             info,
                                                                                             shaderPath,
                                                                                             L"CSMain",
                                                                                             compileArguments,
                                                                                             rootSignature)
        {
        }

        void Dispatch(
            winrt::com_ptr<ID3D12GraphicsCommandList> cmdList,
            uint32_t numParticles,
            const D3D12_GPU_VIRTUAL_ADDRESS &oldPositions,
            const D3D12_GPU_VIRTUAL_ADDRESS &positionsOut,
            const D3D12_GPU_VIRTUAL_ADDRESS &predicted,
            const D3D12_GPU_VIRTUAL_ADDRESS &velocities)
        {
            SetPipelineState(cmdList);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }
    };
}
