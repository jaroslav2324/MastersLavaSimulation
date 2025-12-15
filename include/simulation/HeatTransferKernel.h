#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class HeatTransferReg
    {
        Params = 0,        // b0
        Predicted = 1,     // t1
        SortedIndices = 4, // t4
        CellStart = 5,     // t5
        CellEnd = 6,       // t6
        Temperature = 7,   // t7
        Density = 8,       // t8
        TemperatureOut = 6 // u6
    };

    class HeatTransfer : public ComputeKernelBase
    {
    public:
        HeatTransfer(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &temperature,
            const D3D12_GPU_VIRTUAL_ADDRESS &density,
            const D3D12_GPU_VIRTUAL_ADDRESS &temperatureOut)
        {
            SetPipelineState(cmdList);
            cmdList->SetComputeRootShaderResourceView(1, predicted);
            cmdList->SetComputeRootShaderResourceView(2, sortedIndices);
            cmdList->SetComputeRootShaderResourceView(3, cellStart);
            cmdList->SetComputeRootShaderResourceView(4, cellEnd);
            cmdList->SetComputeRootShaderResourceView(5, temperature);
            cmdList->SetComputeRootShaderResourceView(6, density);
            cmdList->SetComputeRootUnorderedAccessView(7, temperatureOut);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(8);
            rootParams[0].InitAsConstantBufferView(0);
            rootParams[1].InitAsShaderResourceView((UINT)HeatTransferReg::Predicted);
            rootParams[2].InitAsShaderResourceView((UINT)HeatTransferReg::SortedIndices);
            rootParams[3].InitAsShaderResourceView((UINT)HeatTransferReg::CellStart);
            rootParams[4].InitAsShaderResourceView((UINT)HeatTransferReg::CellEnd);
            rootParams[5].InitAsShaderResourceView((UINT)HeatTransferReg::Temperature);
            rootParams[6].InitAsShaderResourceView((UINT)HeatTransferReg::Density);
            rootParams[7].InitAsUnorderedAccessView((UINT)HeatTransferReg::TemperatureOut);
            return rootParams;
        }
    };
}
