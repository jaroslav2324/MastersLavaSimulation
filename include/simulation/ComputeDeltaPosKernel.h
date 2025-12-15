#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class ComputeDeltaPosReg
    {
        Params = 0,        // b0
        Predicted = 1,     // t1
        SortedIndices = 4, // t4
        CellStart = 5,     // t5
        CellEnd = 6,       // t6
        Lambda = 10,       // t10
        DeltaPOut = 2      // u2
    };

    class ComputeDeltaPos : public ComputeKernelBase
    {
    public:
        ComputeDeltaPos(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &predicted,
            const D3D12_GPU_VIRTUAL_ADDRESS &sortedIndices,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellStart,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellEnd,
            const D3D12_GPU_VIRTUAL_ADDRESS &lambdaBuf,
            const D3D12_GPU_VIRTUAL_ADDRESS &deltaPOut)
        {
            SetPipelineState(cmdList);
            cmdList->SetComputeRootShaderResourceView(1, predicted);
            cmdList->SetComputeRootShaderResourceView(2, sortedIndices);
            cmdList->SetComputeRootShaderResourceView(3, cellStart);
            cmdList->SetComputeRootShaderResourceView(4, cellEnd);
            cmdList->SetComputeRootShaderResourceView(5, lambdaBuf);
            cmdList->SetComputeRootUnorderedAccessView(6, deltaPOut);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(7);
            rootParams[0].InitAsConstantBufferView(0);
            rootParams[1].InitAsShaderResourceView((UINT)ComputeDeltaPosReg::Predicted);
            rootParams[2].InitAsShaderResourceView((UINT)ComputeDeltaPosReg::SortedIndices);
            rootParams[3].InitAsShaderResourceView((UINT)ComputeDeltaPosReg::CellStart);
            rootParams[4].InitAsShaderResourceView((UINT)ComputeDeltaPosReg::CellEnd);
            rootParams[5].InitAsShaderResourceView((UINT)ComputeDeltaPosReg::Lambda);
            rootParams[6].InitAsUnorderedAccessView((UINT)ComputeDeltaPosReg::DeltaPOut);
            return rootParams;
        }
    };
}
