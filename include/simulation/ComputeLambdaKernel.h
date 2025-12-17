#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

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

    class ComputeLambda : public SimulationComputeKernelBase
    {
    public:
        ComputeLambda(
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
    };
}
