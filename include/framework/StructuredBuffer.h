#pragma once

#include "pch.h"
#include "DescriptorAllocator.h"

struct StructuredBuffer
{
    winrt::com_ptr<ID3D12Resource> resource = nullptr;
    UINT elementCount = 0;
    UINT elementStride = 0;
    UINT srvIndex = UINT_MAX;
    UINT uavIndex = UINT_MAX;

    void Init(ID3D12Device *device,
              UINT count,
              UINT stride,
              DescriptorAllocator &alloc)
    {
        elementCount = count;
        elementStride = stride;

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(
            UINT64(count) * stride,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(resource.put()));

        // SRV
        srvIndex = alloc.Alloc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Buffer.FirstElement = 0;
        srv.Buffer.NumElements = count;
        srv.Buffer.StructureByteStride = stride;

        device->CreateShaderResourceView(
            resource.get(), &srv, alloc.GetCpuHandle(srvIndex));

        // UAV
        uavIndex = alloc.Alloc();
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = DXGI_FORMAT_UNKNOWN;
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer.FirstElement = 0;
        uav.Buffer.NumElements = count;
        uav.Buffer.StructureByteStride = stride;

        device->CreateUnorderedAccessView(
            resource.get(), nullptr, &uav, alloc.GetCpuHandle(uavIndex));
    }
};