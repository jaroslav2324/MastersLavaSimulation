#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <SimpleMath.h>

#include "Transform.h"
#include "CubeConstBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX::SimpleMath;

class Cube
{
private:
    struct Vertex
    {
        Vector3 position;
    };

    ID3D12Device *device;
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> vertexBufferUpload;
    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> indexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    Transform transform;
    ComPtr<ID3D12Resource> m_constBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS m_constBufferAddress;
    D3D12_CPU_DESCRIPTOR_HANDLE m_constBufferView{};
    CubeConstBuffer m_constBufferData;

public:
    Cube(ID3D12Device *dev);
    ~Cube() = default; // ComPtr автоматически освободит ресурсы

    void Initialize(ID3D12GraphicsCommandList *cmdList);
    void Draw(ID3D12GraphicsCommandList *commandList);

    void CreateConstBuffer(ID3D12Device* device, ID3D12DescriptorHeap* heap, UINT heapIndex);
    void UpdateConstBuffer();

    Transform &GetTransform() { return transform; }
};
