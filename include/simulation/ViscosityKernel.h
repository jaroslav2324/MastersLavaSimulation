#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    enum class ViscosityReg
    {
        Params = 0,       // b0
        Temperature = 7,  // t7
        MuOut = 7,        // u7
        ViscCoeffOut = 13 // u13
    };

    class Viscosity : public ComputeKernelBase
    {
    public:
        Viscosity(
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
            const D3D12_GPU_VIRTUAL_ADDRESS &temperatureIn,
            const D3D12_GPU_VIRTUAL_ADDRESS &muOut,
            const D3D12_GPU_VIRTUAL_ADDRESS &viscCoeffOut)
        {
            SetPipelineState(cmdList);
            cmdList->SetComputeRootShaderResourceView(1, temperatureIn);
            cmdList->SetComputeRootUnorderedAccessView(2, muOut);
            cmdList->SetComputeRootUnorderedAccessView(3, viscCoeffOut);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(4);
            rootParams[0].InitAsConstantBufferView(0);
            rootParams[1].InitAsShaderResourceView((UINT)ViscosityReg::Temperature);
            rootParams[2].InitAsUnorderedAccessView((UINT)ViscosityReg::MuOut);
            rootParams[3].InitAsUnorderedAccessView((UINT)ViscosityReg::ViscCoeffOut);
            return rootParams;
        }
    };
}
