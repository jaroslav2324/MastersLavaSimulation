#pragma once
#include "pch.h"
#include "SimulationComputeKernelBase.h"

namespace SimulationKernels
{
    /**
     * @brief Hash Kernel
     * Computes cell hash for each particle position and outputs hash/index pairs.
     *
     * Input: particle positions (world space)
     * Output: hash values, particle indices
     */
    class CellHash : public SimulationComputeKernelBase
    {
    public:
        CellHash(
            winrt::com_ptr<ID3D12Device> device,
            const GPUSorting::DeviceInfo &info,
            const std::vector<std::wstring> &compileArguments,
            const std::filesystem::path &shaderPath,
            winrt::com_ptr<ID3D12RootSignature> rootSignature) : SimulationComputeKernelBase(device,
                                                                                             info,
                                                                                             shaderPath,
                                                                                             L"CS_HashParticles",
                                                                                             compileArguments,
                                                                                             rootSignature)
        {
        }

        /**
         * @brief Dispatch the spatial hash kernel
         * @param cmdList D3D12 command list
         * @param numParticles Number of particles to hash
         * @param worldMin Minimum corner of simulation domain
         * @param cellSize Size of hash grid cells (typically kernel radius)
         * @param hashTableMask (hashTableSize - 1) for modulo operation
         * @param positionBuffer Input particle positions (float3)
         * @param hashBuffer Output hash values (uint per particle)
         * @param indexBuffer Output particle indices (uint per particle)
         */
        void Dispatch(
            winrt::com_ptr<ID3D12GraphicsCommandList> cmdList,
            uint32_t numParticles)
        {
            SetPipelineState(cmdList);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }
    };
}
