#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class UpdatePositionVelocityReg
    {
        Params = 0,        // b0
        OldPositions = 0,  // t0
        PositionsOut = 12, // u12
        Predicted = 0,     // u0
        Velocities = 1     // u1
    };

    class UpdatePositionVelocity : public ComputeKernelBase
    {
    public:
        UpdatePositionVelocity(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &oldPositions,
            const D3D12_GPU_VIRTUAL_ADDRESS &positionsOut,
            const D3D12_GPU_VIRTUAL_ADDRESS &predicted,
            const D3D12_GPU_VIRTUAL_ADDRESS &velocities)
        {
            SetPipelineState(cmdList);
            cmdList->SetComputeRootShaderResourceView(1, oldPositions);
            cmdList->SetComputeRootUnorderedAccessView(2, positionsOut);
            cmdList->SetComputeRootUnorderedAccessView(3, predicted);
            cmdList->SetComputeRootUnorderedAccessView(4, velocities);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(5);
            rootParams[0].InitAsConstantBufferView(0);
            rootParams[1].InitAsShaderResourceView((UINT)UpdatePositionVelocityReg::OldPositions);
            rootParams[2].InitAsUnorderedAccessView((UINT)UpdatePositionVelocityReg::PositionsOut);
            rootParams[3].InitAsUnorderedAccessView((UINT)UpdatePositionVelocityReg::Predicted);
            rootParams[4].InitAsUnorderedAccessView((UINT)UpdatePositionVelocityReg::Velocities);
            return rootParams;
        }
    };
}
