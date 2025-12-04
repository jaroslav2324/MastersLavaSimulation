#pragma once

#include <d3d12.h>

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

inline static D3D12_RESOURCE_BARRIER GetTransitionCopyToIndex(ID3D12Resource *bufferPtr)
{
    D3D12_RESOURCE_BARRIER transition = GetTransitionCopyToVertex(bufferPtr);
    transition.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    return transition;
}

inline static D3D12_DESCRIPTOR_RANGE CreateDescriptorRange(
    D3D12_DESCRIPTOR_RANGE_TYPE type,
    UINT numDescriptors,
    UINT baseShaderRegister,
    UINT registerSpace = 0,
    UINT offset = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
{
    D3D12_DESCRIPTOR_RANGE r = {};
    r.RangeType = type;
    r.NumDescriptors = numDescriptors;
    r.BaseShaderRegister = baseShaderRegister;
    r.RegisterSpace = registerSpace;
    r.OffsetInDescriptorsFromTableStart = offset;
    return r;
}

inline static D3D12_ROOT_PARAMETER CreateDescriptorTableRootParam(const D3D12_DESCRIPTOR_RANGE *ranges, UINT numRanges, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL)
{
    D3D12_ROOT_PARAMETER p = {};
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.DescriptorTable.NumDescriptorRanges = numRanges;
    p.DescriptorTable.pDescriptorRanges = ranges;
    p.ShaderVisibility = vis;
    return p;
}

inline static D3D12_ROOT_PARAMETER CreateCBVRootParam(UINT shaderRegister, UINT registerSpace = 0, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL)
{
    D3D12_ROOT_PARAMETER p = {};
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.Descriptor.ShaderRegister = shaderRegister;
    p.Descriptor.RegisterSpace = registerSpace;
    p.ShaderVisibility = vis;
    return p;
}

// Parameterized heap properties helper
inline static D3D12_HEAP_PROPERTIES MakeHeapProperties(D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT, UINT creationNodeMask = 1, UINT visibleNodeMask = 1)
{
    D3D12_HEAP_PROPERTIES props = {};
    props.Type = type;
    props.CreationNodeMask = creationNodeMask;
    props.VisibleNodeMask = visibleNodeMask;
    return props;
}

// Helper to create a UAV buffer description for RWStructuredBuffer or raw buffer
inline static D3D12_UNORDERED_ACCESS_VIEW_DESC MakeBufferUAVDesc(UINT numElements, UINT stride = sizeof(uint32_t), DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC d = {};
    d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    d.Format = format;
    d.Buffer.FirstElement = 0;
    d.Buffer.NumElements = numElements;
    d.Buffer.StructureByteStride = stride;
    d.Buffer.CounterOffsetInBytes = 0;
    d.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    return d;
}