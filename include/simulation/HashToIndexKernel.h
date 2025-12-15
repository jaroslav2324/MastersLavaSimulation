#pragma once
#include "pch.h"
#include "GPUSorting/ComputeKernelBase.h"

namespace SimulationKernels
{
    // Root signature parameter indices for HashToIndex
    // Matches registers in shaders/HashToIndex.hlsl
    enum class HashToIndexReg
    {
        Params = 0,     // b0
        Hashes = 3,     // t3
        CellStart = 10, // u10
        CellEnd = 11    // u11
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
                                                                         L"CS_FindCellRanges",
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
            const D3D12_GPU_VIRTUAL_ADDRESS &cellStart,
            const D3D12_GPU_VIRTUAL_ADDRESS &cellEnd)
        {
            SetPipelineState(cmdList);
            // Caller should bind SimParams CBV at root 0 via SetComputeRootConstantBufferView
            cmdList->SetComputeRootShaderResourceView(1, hashes);
            cmdList->SetComputeRootUnorderedAccessView(2, cellStart);
            cmdList->SetComputeRootUnorderedAccessView(3, cellEnd);

            uint32_t threadGroups = (numParticles + 255) / 256;
            cmdList->Dispatch(threadGroups, 1, 1);
        }

    protected:
        const std::vector<CD3DX12_ROOT_PARAMETER1> CreateRootParameters() override
        {
            auto rootParams = std::vector<CD3DX12_ROOT_PARAMETER1>(4);
            rootParams[0].InitAsConstantBufferView(0); // SimParams (b0)
            rootParams[1].InitAsShaderResourceView((UINT)HashToIndexReg::Hashes);
            rootParams[2].InitAsUnorderedAccessView((UINT)HashToIndexReg::CellStart);
            rootParams[3].InitAsUnorderedAccessView((UINT)HashToIndexReg::CellEnd);
            return rootParams;
        }
    };
}
