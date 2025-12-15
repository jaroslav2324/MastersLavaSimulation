#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    // Root signature parameter indices for Hash
    enum class SpatialHashReg
    {
        Params = 0,
        PositionBuffer = 1,
        HashBuffer = 2,
        IndexBuffer = 3,
    };

    /**
     * @brief Hash Kernel
     * Computes cell hash for each particle position and outputs hash/index pairs.
     *
     * Input: particle positions (world space)
     * Output: hash values, particle indices
     */
    class CellHash : public ComputeKernelBase
    {
    public:
        CellHash(
            winrt::com_ptr<ID3D12Device> device,
            const GPUSorting::DeviceInfo &info,
            const std::vector<std::wstring> &compileArguments,
            const std::filesystem::path &shaderPath) : ComputeKernelBase(device,
                                                                         info,
                                                                         shaderPath,
                                                                         L"CS_HashParticles",
                                                                         compileArguments,
                                                                         CreateRootParameters())
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
            uint32_t numParticles,
            const Vector3 &worldMin,
            float cellSize,
            uint32_t hashTableMask,
            const D3D12_GPU_VIRTUAL_ADDRESS &positionBuffer,
            const D3D12_GPU_VIRTUAL_ADDRESS &hashBuffer,
            const D3D12_GPU_VIRTUAL_ADDRESS &indexBuffer)
        {
            // Constant buffer: [worldMin(f32x3), cellSize(f32), numParticles(u32), hashTableMask(u32)]
            std::array<uint32_t, 6> constants;
            memcpy(constants.data(), &worldMin.x, sizeof(Vector3));
            memcpy(reinterpret_cast<float *>(constants.data()) + 3, &cellSize, sizeof(float));
            constants[4] = numParticles;
            constants[5] = hashTableMask;

            SetPipelineState(cmdList);
            cmdList->SetComputeRoot32BitConstants(0, (uint32_t)constants.size(), constants.data(), 0);
            cmdList->SetComputeRootShaderResourceView(1, positionBuffer);
            cmdList->SetComputeRootUnorderedAccessView(2, hashBuffer);
            cmdList->SetComputeRootUnorderedAccessView(3, indexBuffer);

            // Dispatch in groups of 256 threads
            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(4);
            rootParams[0].InitAsConstants(6, 0); // [worldMin(3*f32), cellSize(f32), numParticles(u32), hashTableMask(u32)]
            rootParams[1].InitAsShaderResourceView((UINT)SpatialHashReg::PositionBuffer);
            rootParams[2].InitAsUnorderedAccessView((UINT)SpatialHashReg::HashBuffer);
            rootParams[3].InitAsUnorderedAccessView((UINT)SpatialHashReg::IndexBuffer);
            return rootParams;
        }
    };
}
