#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    // Root signature parameter indices for HashToIndex
    enum class HashToIndexReg
    {
        Params = 0,
        Hashes = 1,
        CellStart = 2,
    };

    /**
     * @brief Hash To Index Kernel
     * Finds the starting index for each cell by detecting hash transitions.
     * After particles are sorted by hash, this identifies where each cell's
     * particles begin in the sorted list.
     *
     * Input: sorted hash values (from GPUSorting)
     * Output: cell start indices (one entry per cell containing particle index)
     */
    class HashToIndex : public ComputeKernelBase
    {
    public:
        HashToIndex(
            winrt::com_ptr<ID3D12Device> device,
            const GPUSorting::DeviceInfo &info,
            const std::vector<std::wstring> &compileArguments,
            const std::filesystem::path &shaderPath) : ComputeKernelBase(device,
                                                                         info,
                                                                         shaderPath,
                                                                         L"CS_FindCellStart",
                                                                         compileArguments,
                                                                         CreateRootParameters())
        {
        }

        /**
         * @brief Dispatch the hash-to-index kernel
         *
         * Prerequisite: The hashes buffer must be pre-sorted by one of the
         * GPUSorting algorithms (DeviceRadixSort, OneSweep, etc.).
         *
         * @param cmdList D3D12 command list
         * @param numParticles Number of particles (hashes.Length)
         * @param hashes Input sorted hash values (StructuredBuffer<uint>)
         * @param cellStart Output cell start indices (RWStructuredBuffer<int>)
         */
        void Dispatch(
            winrt::com_ptr<ID3D12GraphicsCommandList> cmdList,
            uint32_t numParticles,
            const D3D12_GPU_VIRTUAL_ADDRESS &hashes,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellStart)
        {
            // Constant buffer: [numParticles(u32)]
            std::array<uint32_t, 1> constants = {numParticles};

            SetPipelineState(cmdList);
            cmdList->SetComputeRoot32BitConstants(0, (uint32_t)constants.size(), constants.data(), 0);
            cmdList->SetComputeRootShaderResourceView(1, hashes);
            cmdList->SetComputeRootUnorderedAccessView(2, cellStart);

            // Dispatch in groups of 256 threads
            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(3);
            rootParams[0].InitAsConstants(1, 0); // [numParticles]
            rootParams[1].InitAsShaderResourceView((UINT)HashToIndexReg::Hashes);
            rootParams[2].InitAsUnorderedAccessView((UINT)HashToIndexReg::CellStart);
            return rootParams;
        }
    };
}
