#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

namespace SimulationKernels
{
    class Viscosity : public SimulationComputeKernelBase
    {
    public:
        Viscosity(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &temperatureIn,
            const D3D12_GPU_VIRTUAL_ADDRESS &muOut,
            const D3D12_GPU_VIRTUAL_ADDRESS &viscCoeffOut)
        {
            SetPipelineState(cmdList);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }
    };
}
