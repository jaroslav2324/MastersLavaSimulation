#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <SimpleMath.h>
#include "Transform.h"
#include "d3dx12.h"

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

public:
    Cube(ID3D12Device *dev);
    ~Cube() = default; // ComPtr автоматически освободит ресурсы

    void Initialize(ID3D12GraphicsCommandList *cmdList);
    void Draw(ID3D12GraphicsCommandList *commandList);

    Transform &GetTransform() { return transform; }
};
