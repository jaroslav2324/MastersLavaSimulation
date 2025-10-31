#pragma once

#include <d3d12.h>

// TODO: replace with CD3DX12
inline static D3D12_HEAP_PROPERTIES GetDefaultHeapProperties()
{
    D3D12_HEAP_PROPERTIES properties = {};
    properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

// TODO: replace with CD3DX12
inline static D3D12_HEAP_PROPERTIES GetUploadHeapProperties()
{
    D3D12_HEAP_PROPERTIES properties = GetDefaultHeapProperties();
    properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    return properties;
}

// TODO: replace with CD3DX12
inline static D3D12_RESOURCE_DESC GetBufferResourceDesc(size_t bufferSize)
{
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Alignment = 0;
    bufDesc.Width = bufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return bufDesc;
}

// TODO: replace with CD3DX12
inline static D3D12_RESOURCE_BARRIER GetTransitionCopyToVertex(ID3D12Resource *bufferPtr)
{
    D3D12_RESOURCE_BARRIER transition = {};
    transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transition.Transition.pResource = bufferPtr;
    transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    transition.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return transition;
}

// TODO: replace with CD3DX12
inline static D3D12_RESOURCE_BARRIER GetTransitionCopyToIndex(ID3D12Resource *bufferPtr)
{
    D3D12_RESOURCE_BARRIER transition = GetTransitionCopyToVertex(bufferPtr);
    transition.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    return transition;
}