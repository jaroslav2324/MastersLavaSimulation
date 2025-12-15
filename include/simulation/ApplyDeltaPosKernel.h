#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class ApplyDeltaPosReg
    {
        Params = 0,      // b0
        PredictedRW = 0, // u0
        DeltaP = 2       // u2
    };

    class ApplyDeltaPos : public ComputeKernelBase
    {
    public:
        ApplyDeltaPos(
            winrt::com_ptr<ID3D12Device> device,
            const GPUSorting::DeviceInfo &info,
            const std::vector<std::wstring> &compileArguments,
            const std::filesystem::path &shaderPath) : ComputeKernelBase(device,
                                                                         info,
                                                                         shaderPath,
                                                                         L"CSMain",
                                                                         compileArguments,
                                                                         CreateRootParameters())
        {
        }

        void Dispatch(
            winrt::com_ptr<ID3D12GraphicsCommandList> cmdList,
            uint32_t numParticles,
            const D3D12_GPU_VIRTUAL_ADDRESS &predictedRW,
            const D3D12_GPU_VIRTUAL_ADDRESS &deltaP)
        {
            SetPipelineState(cmdList);
            cmdList->SetComputeRootUnorderedAccessView(1, predictedRW);
            cmdList->SetComputeRootUnorderedAccessView(2, deltaP);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(3);
            rootParams[0].InitAsConstantBufferView(0);
            rootParams[1].InitAsUnorderedAccessView((UINT)ApplyDeltaPosReg::PredictedRW);
            rootParams[2].InitAsUnorderedAccessView((UINT)ApplyDeltaPosReg::DeltaP);
            return rootParams;
        }
    };
}
