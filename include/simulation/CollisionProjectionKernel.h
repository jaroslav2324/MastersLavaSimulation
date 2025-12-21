#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

namespace SimulationKernels
{
    enum class CollisionProjectionReg
    {
        Params = 0,             // b0
        PredictedPositions = 0, // u0
        Velocity = 1            // u1
    };

    class CollisionProjection : public SimulationComputeKernelBase
    {
    public:
        CollisionProjection(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &predictedPositions,
            const D3D12_GPU_VIRTUAL_ADDRESS &velocity)
        {
            SetPipelineState(cmdList);
            // Caller must bind SimParams CBV at root 0
            cmdList->SetComputeRootUnorderedAccessView(1, predictedPositions);
            cmdList->SetComputeRootUnorderedAccessView(2, velocity);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }
    };
}
