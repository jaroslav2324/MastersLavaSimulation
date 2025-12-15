#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class ComputeLambdaReg
    {
        Params = 0,        // b0
        Predicted = 0,     // t0
        SortedIndices = 1, // t1
        CellStart = 2,     // t2
        CellEnd = 3,       // t3
        Density = 8,       // t8
        ConstraintC = 9,   // t9
        LambdaOut = 10     // u10
    };

    class ComputeLambda : public ComputeKernelBase
    {
    public:
        ComputeLambda(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &density,
            const D3D12_GPU_VIRTUAL_ADDRESS &constraintC,
            const D3D12_GPU_VIRTUAL_ADDRESS &lambdaOut)
        {
            SetPipelineState(cmdList);
            // Caller binds SimParams CBV at root 0
            cmdList->SetComputeRootShaderResourceView(1, predicted);
            cmdList->SetComputeRootShaderResourceView(2, sortedIndices);
            cmdList->SetComputeRootShaderResourceView(3, cellStart);
            cmdList->SetComputeRootShaderResourceView(4, cellEnd);
            cmdList->SetComputeRootShaderResourceView(5, density);
            cmdList->SetComputeRootShaderResourceView(6, constraintC);
            cmdList->SetComputeRootUnorderedAccessView(7, lambdaOut);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(8);
            rootParams[0].InitAsConstantBufferView(0);
            rootParams[1].InitAsShaderResourceView((UINT)ComputeLambdaReg::Predicted);
            rootParams[2].InitAsShaderResourceView((UINT)ComputeLambdaReg::SortedIndices);
            rootParams[3].InitAsShaderResourceView((UINT)ComputeLambdaReg::CellStart);
            rootParams[4].InitAsShaderResourceView((UINT)ComputeLambdaReg::CellEnd);
            rootParams[5].InitAsShaderResourceView((UINT)ComputeLambdaReg::Density);
            rootParams[6].InitAsShaderResourceView((UINT)ComputeLambdaReg::ConstraintC);
            rootParams[7].InitAsUnorderedAccessView((UINT)ComputeLambdaReg::LambdaOut);
            return rootParams;
        }
    };
}
