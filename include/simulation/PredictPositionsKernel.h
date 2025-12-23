#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

namespace SimulationKernels
{
    /**
     * @brief Predict Positions Kernel
     * Applies external forces (gravity) and predicts new particle positions.
     * Input: current positions, current velocities
     * Output: predicted positions, updated velocities
     */
    class PredictPositions : public SimulationComputeKernelBase
    {
    public:
        PredictPositions(
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
            uint32_t particleCount)
        {
            SetPipelineState(cmdList);

            uint32_t threadGroups = (particleCount + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }
    };
}
