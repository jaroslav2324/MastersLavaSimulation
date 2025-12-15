#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class ComputeDensityReg
    {
        Params = 0,        // b0
        Predicted = 1,     // t1
        SortedIndices = 4, // t4
        CellStart = 5,     // t5
        CellEnd = 6,       // t6
        DensityOut = 3,    // u3
        ConstraintOut = 4  // u4
    };

    class ComputeDensity : public ComputeKernelBase
    {
    public:
        ComputeDensity(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &predictedPositions,
            const D3D12_GPU_VIRTUAL_ADDRESS &sortedIndices,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellStart,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellEnd,
            const D3D12_GPU_VIRTUAL_ADDRESS &densityOut,
            const D3D12_GPU_VIRTUAL_ADDRESS &constraintOut)
        {
            SetPipelineState(cmdList);
            // Caller binds SimParams CBV at root 0
            cmdList->SetComputeRootShaderResourceView(1, predictedPositions);
            cmdList->SetComputeRootShaderResourceView(2, sortedIndices);
            cmdList->SetComputeRootShaderResourceView(3, cellStart);
            cmdList->SetComputeRootShaderResourceView(4, cellEnd);
            cmdList->SetComputeRootUnorderedAccessView(5, densityOut);
            cmdList->SetComputeRootUnorderedAccessView(6, constraintOut);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(7);
            rootParams[0].InitAsConstantBufferView(0); // SimParams
            rootParams[1].InitAsShaderResourceView((UINT)ComputeDensityReg::Predicted);
            rootParams[2].InitAsShaderResourceView((UINT)ComputeDensityReg::SortedIndices);
            rootParams[3].InitAsShaderResourceView((UINT)ComputeDensityReg::CellStart);
            rootParams[4].InitAsShaderResourceView((UINT)ComputeDensityReg::CellEnd);
            rootParams[5].InitAsUnorderedAccessView((UINT)ComputeDensityReg::DensityOut);
            rootParams[6].InitAsUnorderedAccessView((UINT)ComputeDensityReg::ConstraintOut);
            return rootParams;
        }
    };
}
