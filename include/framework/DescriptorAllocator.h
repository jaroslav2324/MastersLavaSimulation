#pragma once

#include "pch.h"

class DescriptorAllocator
{
public:
    void Init(
        ID3D12Device *device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT maxDescriptors,
        bool shaderVisible)
    {
        m_device = device;
        m_type = type;
        m_descriptorSize =
            device->GetDescriptorHandleIncrementSize(type);

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = maxDescriptors;
        desc.Type = type;
        desc.Flags = shaderVisible
                         ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                         : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        device->CreateDescriptorHeap(
            &desc,
            IID_PPV_ARGS(&m_heap));

        m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible)
            m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();

        m_capacity = maxDescriptors;
        m_allocated = 0;
    }

    /// Возвращает индекс дескриптора
    UINT Alloc()
    {
        assert(m_allocated < m_capacity);
        return m_allocated++;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(UINT index) const
    {
        return {
            m_cpuStart.ptr + index * m_descriptorSize};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(UINT index) const
    {
        return {
            m_gpuStart.ptr + index * m_descriptorSize};
    }

    ID3D12DescriptorHeap *GetHeap() const
    {
        return m_heap.get();
    }

private:
    ID3D12Device *m_device = nullptr;
    winrt::com_ptr<ID3D12DescriptorHeap> m_heap;

    D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
    UINT m_descriptorSize = 0;

    UINT m_capacity = 0;
    UINT m_allocated = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
};