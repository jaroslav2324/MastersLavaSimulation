#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    // Root signature parameter indices for PredictPositions
    enum class PredictPositionsReg
    {
        Params = 0,
        PositionsSrc = 1,
        VelocitySrc = 2,
        PredictedPositionsDst = 3,
        VelocityDst = 4,
    };

    /**
     * @brief Predict Positions Kernel
     * Applies external forces (gravity) and predicts new particle positions.
     * Input: current positions, current velocities
     * Output: predicted positions, updated velocities
     */
    class PredictPositions : public ComputeKernelBase
    {
    public:
        PredictPositions(
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

        /**
         * @brief Dispatch the prediction kernel
         * @param cmdList D3D12 command list
         * @param particleCount Number of particles to process
         * @param dt Time step
         * @param externalForce External force vector (typically gravity)
         * @param positionsSrc Input particle positions (x, y, z, radius)
         * @param velocitySrc Input particle velocities (vx, vy, vz, pad)
         * @param predictedPositionsDst Output predicted positions
         * @param velocityDst Output updated velocities
         */
        void Dispatch(
            winrt::com_ptr<ID3D12GraphicsCommandList> cmdList,
            uint32_t particleCount,
            float dt,
            const DirectX::XMFLOAT3 &externalForce, // TODO: use simple math
            const D3D12_GPU_VIRTUAL_ADDRESS &positionsSrc,
            const D3D12_GPU_VIRTUAL_ADDRESS &velocitySrc,
            const D3D12_GPU_VIRTUAL_ADDRESS &predictedPositionsDst,
            const D3D12_GPU_VIRTUAL_ADDRESS &velocityDst)
        {
            // Constant buffer: [particleCount(u32), dt(f32), externalForce(f32x3)]
            std::array<float, 5> constants = {
                static_cast<float>(particleCount),
                dt,
                externalForce.x,
                externalForce.y,
                externalForce.z};

            SetPipelineState(cmdList);
            cmdList->SetComputeRoot32BitConstants(0, (uint32_t)constants.size(), constants.data(), 0);
            cmdList->SetComputeRootShaderResourceView(1, positionsSrc);
            cmdList->SetComputeRootShaderResourceView(2, velocitySrc);
            cmdList->SetComputeRootUnorderedAccessView(3, predictedPositionsDst);
            cmdList->SetComputeRootUnorderedAccessView(4, velocityDst);

            // Dispatch in groups of 256 threads
            uint32_t threadGroups = (particleCount + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(5);
            rootParams[0].InitAsConstants(5, 0); // [particleCount, dt, fx, fy, fz]
            rootParams[1].InitAsShaderResourceView((UINT)PredictPositionsReg::PositionsSrc);
            rootParams[2].InitAsShaderResourceView((UINT)PredictPositionsReg::VelocitySrc);
            rootParams[3].InitAsUnorderedAccessView((UINT)PredictPositionsReg::PredictedPositionsDst);
            rootParams[4].InitAsUnorderedAccessView((UINT)PredictPositionsReg::VelocityDst);
            return rootParams;
        }
    };
}
