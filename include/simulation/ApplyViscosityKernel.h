#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

namespace SimulationKernels
{
    enum class ApplyViscosityReg
    {
        Params = 0,        // b0
        Predicted = 1,     // t1
        VelocitiesIn = 2,  // t2
        SortedIndices = 4, // t4
        CellStart = 5,     // t5
        CellEnd = 6,       // t6
        ViscCoeff = 11,    // t11
        VelocitiesOut = 1  // u1
    };

    class ApplyViscosity : public SimulationComputeKernelBase
    {
    public:
        ApplyViscosity(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &velocitiesIn,
            const D3D12_GPU_VIRTUAL_ADDRESS &sortedIndices,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellStart,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellEnd,
            const D3D12_GPU_VIRTUAL_ADDRESS &viscCoeff,
            const D3D12_GPU_VIRTUAL_ADDRESS &velocitiesOut)
        {
            SetPipelineState(cmdList);
            cmdList->SetComputeRootShaderResourceView(1, predicted);
            cmdList->SetComputeRootShaderResourceView(2, velocitiesIn);
            cmdList->SetComputeRootShaderResourceView(3, sortedIndices);
            cmdList->SetComputeRootShaderResourceView(4, cellStart);
            cmdList->SetComputeRootShaderResourceView(5, cellEnd);
            cmdList->SetComputeRootShaderResourceView(6, viscCoeff);
            cmdList->SetComputeRootUnorderedAccessView(7, velocitiesOut);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }
    };
}
