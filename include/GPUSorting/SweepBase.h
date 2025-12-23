/******************************************************************************
 * GPUSorting
 *
 * SPDX-License-Identifier: MIT
 * Copyright Thomas Smith 4/22/2024
 * https://github.com/b0nes164/GPUSorting
 *
 ******************************************************************************/
#pragma once
#include "pch.h"
#include "GPUSorting.h"
#include "GPUSortBase.h"
#include "SweepCommonKernels.h"

class SweepBase : public GPUSortBase
{
protected:
    const uint32_t k_globalHistPartitionSize = 32768;

    winrt::com_ptr<ID3D12Resource> m_indexBuffer;
    winrt::com_ptr<ID3D12Resource> m_passHistBuffer;
    winrt::com_ptr<ID3D12Resource> m_globalHistBuffer;

    SweepCommonKernels::InitSweep *m_initSweep = nullptr;
    SweepCommonKernels::GlobalHist *m_globalHist = nullptr;
    SweepCommonKernels::Scan *m_scan = nullptr;
    SweepCommonKernels::DigitBinningPass *m_digitPass = nullptr;

    uint32_t m_globalHistPartitions;

public:
    // Allow the caller to override internal buffers; these methods forward
    // to GPUSortBase setters and will prevent this class from allocating
    // or freeing these buffers if passed with userProvided=true.
    void SetSortBuffer(winrt::com_ptr<ID3D12Resource> buffer, bool userProvided = true)
    {
        GPUSortBase::SetSortBuffer(buffer, userProvided);
    }

    void SetSortPayloadBuffer(winrt::com_ptr<ID3D12Resource> buffer, bool userProvided = true)
    {
        GPUSortBase::SetSortPayloadBuffer(buffer, userProvided);
    }

    void SetAltBuffer(winrt::com_ptr<ID3D12Resource> buffer, bool userProvided = true)
    {
        GPUSortBase::SetAltBuffer(buffer, userProvided);
    }

    void SetAltPayloadBuffer(winrt::com_ptr<ID3D12Resource> buffer, bool userProvided = true)
    {
        GPUSortBase::SetAltPayloadBuffer(buffer, userProvided);
    }

    void SetAllBuffers(
        winrt::com_ptr<ID3D12Resource> sortBuffer,
        winrt::com_ptr<ID3D12Resource> sortPayloadBuffer,
        winrt::com_ptr<ID3D12Resource> altBuffer,
        winrt::com_ptr<ID3D12Resource> altPayloadBuffer,
        bool userProvided = true)
    {
        GPUSortBase::SetAllBuffers(sortBuffer, sortPayloadBuffer, altBuffer, altPayloadBuffer, userProvided);
    }

    SweepBase(
        winrt::com_ptr<ID3D12Device> _device,
        GPUSorting::DeviceInfo _deviceInfo,
        GPUSorting::ORDER sortingOrder,
        GPUSorting::KEY_TYPE keyType,
        const char *sortName,
        uint32_t radixPasses,
        uint32_t radix,
        uint32_t maxReadBack) : GPUSortBase(_device,
                                            _deviceInfo,
                                            sortingOrder,
                                            keyType,
                                            sortName,
                                            radixPasses,
                                            radix,
                                            maxReadBack)
    {
        // TODO: better exception handling
        if (!m_devInfo.SupportsOneSweep)
            printf("Warning this device does not support Sweep family sorting, correct execution is not guarunteed");
    }

    SweepBase(
        winrt::com_ptr<ID3D12Device> _device,
        GPUSorting::DeviceInfo _deviceInfo,
        GPUSorting::ORDER sortingOrder,
        GPUSorting::KEY_TYPE keyType,
        GPUSorting::PAYLOAD_TYPE payloadType,
        const char *sortName,
        uint32_t radixPasses,
        uint32_t radix,
        uint32_t maxReadBack) : GPUSortBase(_device,
                                            _deviceInfo,
                                            sortingOrder,
                                            keyType,
                                            payloadType,
                                            sortName,
                                            radixPasses,
                                            radix,
                                            maxReadBack)
    {
        // TODO: better exception handling
        if (!m_devInfo.SupportsOneSweep)
            printf("Warning this device does not support Sweep family sorting, correct execution is not guarunteed");
    }

    ~SweepBase()
    {
    }

    void UpdateSize(uint32_t size, bool disposeBuffers = true) override
    {
        if (m_numKeys != size)
        {
            m_numKeys = size;
            m_partitions = divRoundUp(m_numKeys, k_tuningParameters.partitionSize);
            m_globalHistPartitions = divRoundUp(m_numKeys, k_globalHistPartitionSize);
            if (disposeBuffers)
            {
                DisposeBuffers();
            }
            InitBuffers(m_numKeys, m_partitions);
        }
    }

    bool TestAll() override
    {
        printf("Beginning ");
        printf(k_sortName);
        PrintSortingConfig(k_sortingConfig);
        printf("test all. \n");

        uint32_t sortPayloadTestsPassed = 0;
        uint32_t testsExpected = k_tuningParameters.partitionSize + 1 + 3;

        const uint32_t testEnd = k_tuningParameters.partitionSize * 2 + 1;
        for (uint32_t i = k_tuningParameters.partitionSize; i < testEnd; ++i)
        {
            sortPayloadTestsPassed += ValidateSort(i, i);

            if (!(i & 127))
                printf(".");
        }

        printf("\n");
        printf("%u / %u passed. \n", sortPayloadTestsPassed, k_tuningParameters.partitionSize + 1);

        // Validate the multi-dispatching approach to handle large inputs.
        // This has extremely large memory requirements. So we check to make
        // sure we can do it.
        printf("Beginning large size tests\n");
        sortPayloadTestsPassed += ValidateSort(1 << 21, 5);
        sortPayloadTestsPassed += ValidateSort(1 << 22, 7);
        sortPayloadTestsPassed += ValidateSort(1 << 23, 11);

        uint64_t totalAvailableMemory = m_devInfo.dedicatedVideoMemory + m_devInfo.sharedSystemMemory;
        uint32_t maxDimTestSize = k_maxDispatchDimension * k_tuningParameters.partitionSize; // does not exceed u32 for all tuning paramters.

        uint64_t staticMemoryRequirements =
            ((uint64_t)k_radix * k_radixPasses * sizeof(uint32_t)) + // This is the global histogram
            (sizeof(uint32_t)) +                                     // The error buffer
            k_maxReadBack * sizeof(uint32_t);                        // The readback buffer

        // Multiply by 4 for sort, payload, alt, alt payload, add 1
        // in case fragmentation of the memory causes issues when spilling into shared system memory.
        uint64_t pairsMemoryRequirements =
            ((uint64_t)k_maxDispatchDimension * k_tuningParameters.partitionSize * sizeof(uint32_t) * 5) +
            staticMemoryRequirements +
            ((1 << 20) * sizeof(uint32_t));

        if (totalAvailableMemory >= pairsMemoryRequirements)
        {
            sortPayloadTestsPassed += ValidateSort(maxDimTestSize - 1, 13);
            sortPayloadTestsPassed += ValidateSort(maxDimTestSize, 17);
            sortPayloadTestsPassed += ValidateSort(maxDimTestSize + (1 << 20), 19);
            testsExpected += 3;
        }
        else
        {
            printf("Warning, device does not have enough memory to test multi-dispatch");
            printf(" handling of very large inputs. These tests have been skipped\n");
        }

        if (sortPayloadTestsPassed == testsExpected)
        {
            printf("%u / %u  All tests passed. \n\n", testsExpected, testsExpected);
            return true;
        }
        else
        {
            printf("%u / %u  Test failed. \n\n", sortPayloadTestsPassed, testsExpected);
            return false;
        }
    }

protected:
    void DisposeBuffers() override
    {
        if (!m_userProvidedSortBuffer)
            m_sortBuffer = nullptr;
        if (!m_userProvidedSortPayloadBuffer)
            m_sortPayloadBuffer = nullptr;
        if (!m_userProvidedAltBuffer)
            m_altBuffer = nullptr;
        if (!m_userProvidedAltPayloadBuffer)
            m_altPayloadBuffer = nullptr;
        m_passHistBuffer = nullptr;
    }

    void InitStaticBuffers() override
    {
        m_globalHistBuffer = CreateBuffer(
            m_device,
            k_radix * k_radixPasses * sizeof(uint32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        m_indexBuffer = CreateBuffer(
            m_device,
            k_radixPasses * sizeof(uint32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        m_errorCountBuffer = CreateBuffer(
            m_device,
            1 * sizeof(uint32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        m_readBackBuffer = CreateBuffer(
            m_device,
            k_maxReadBack * sizeof(uint32_t),
            D3D12_HEAP_TYPE_READBACK,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_FLAG_NONE);
    }

    void InitBuffers(const uint32_t numKeys, const uint32_t threadBlocks) override
    {
        if (!m_userProvidedSortBuffer)
            m_sortBuffer = CreateBuffer(
                m_device,
                numKeys * sizeof(uint32_t),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (!m_userProvidedAltBuffer)
            m_altBuffer = CreateBuffer(
                m_device,
                numKeys * sizeof(uint32_t),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        m_passHistBuffer = CreateBuffer(
            m_device,
            k_radix * k_radixPasses * threadBlocks * sizeof(uint32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        if (k_sortingConfig.sortingMode == GPUSorting::MODE_PAIRS)
        {
            if (!m_userProvidedSortPayloadBuffer)
                m_sortPayloadBuffer = CreateBuffer(
                    m_device,
                    numKeys * sizeof(uint32_t),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            if (!m_userProvidedAltPayloadBuffer)
                m_altPayloadBuffer = CreateBuffer(
                    m_device,
                    numKeys * sizeof(uint32_t),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        }
        else
        {
            if (!m_userProvidedSortPayloadBuffer)
                m_sortPayloadBuffer = CreateBuffer(
                    m_device,
                    1 * sizeof(uint32_t),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            if (!m_userProvidedAltPayloadBuffer)
                m_altPayloadBuffer = CreateBuffer(
                    m_device,
                    1 * sizeof(uint32_t),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        }
    }

    void PrepareSortCmdList() override
    {
        assert(m_initSweep && "m_initSweep is not initialized");
        assert(m_globalHist && "m_globalHist is not initialized");
        assert(m_scan && "m_scan is not initialized");
        assert(m_digitPass && "m_digitPass is not initialized");

        m_initSweep->Dispatch(
            m_cmdList,
            m_globalHistBuffer->GetGPUVirtualAddress(),
            m_passHistBuffer->GetGPUVirtualAddress(),
            m_indexBuffer->GetGPUVirtualAddress(),
            m_partitions);
        UAVBarrierSingle(m_cmdList, m_globalHistBuffer);

        m_globalHist->Dispatch(
            m_cmdList,
            m_sortBuffer->GetGPUVirtualAddress(),
            m_globalHistBuffer->GetGPUVirtualAddress(),
            m_numKeys,
            m_globalHistPartitions);
        UAVBarrierSingle(m_cmdList, m_globalHistBuffer);

        m_scan->Dispatch(
            m_cmdList,
            m_globalHistBuffer->GetGPUVirtualAddress(),
            m_passHistBuffer->GetGPUVirtualAddress(),
            m_partitions,
            k_radixPasses);
        UAVBarrierSingle(m_cmdList, m_passHistBuffer);

        for (uint32_t radixShift = 0; radixShift < 32; radixShift += 8)
        {
            m_digitPass->Dispatch(
                m_cmdList,
                m_sortBuffer->GetGPUVirtualAddress(),
                m_altBuffer->GetGPUVirtualAddress(),
                m_sortPayloadBuffer->GetGPUVirtualAddress(),
                m_altPayloadBuffer->GetGPUVirtualAddress(),
                m_indexBuffer->GetGPUVirtualAddress(),
                m_passHistBuffer,
                m_numKeys,
                m_partitions,
                radixShift);
            UAVBarrierSingle(m_cmdList, m_sortBuffer);
            UAVBarrierSingle(m_cmdList, m_sortPayloadBuffer);
            UAVBarrierSingle(m_cmdList, m_altBuffer);
            UAVBarrierSingle(m_cmdList, m_altPayloadBuffer);

            swap(m_sortBuffer, m_altBuffer);
            swap(m_sortPayloadBuffer, m_altPayloadBuffer);
        }
    }
};