#pragma once

#include "pch.h"

#include "Transform.h"
#include "CubeConstBuffer.h"

using winrt::com_ptr;
using namespace DirectX::SimpleMath;

class Cube
{
private:
    struct Vertex
    {
        Vector4 position;
    };

    ID3D12Device *device;
    com_ptr<ID3D12Resource> vertexBuffer;
    com_ptr<ID3D12Resource> vertexBufferUpload;
    com_ptr<ID3D12Resource> indexBuffer;
    com_ptr<ID3D12Resource> indexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    Transform transform;
    com_ptr<ID3D12Resource> m_constBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS m_constBufferAddress;
    D3D12_CPU_DESCRIPTOR_HANDLE m_constBufferView{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_constBufferGPUHandle{}; // GPU handle for root descriptor table
    CubeConstBuffer m_constBufferData;

public:
    Cube() = default;
    ~Cube() = default;

    void Initialize(ID3D12Device *dev, ID3D12GraphicsCommandList *cmdList);
    void Draw(ID3D12GraphicsCommandList *commandList);

    void CreateConstBuffer(ID3D12Device *device, ID3D12DescriptorHeap *heap, UINT heapIndex);
    void UpdateConstBuffer();

    Transform &GetTransform() { return transform; }
};
