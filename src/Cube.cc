#include "Cube.h"

Cube::Cube(ID3D12Device *dev) : device(dev)
{
}

void Cube::Initialize(ID3D12GraphicsCommandList *commandList)
{
    // Vertices for a unit cube
    Vertex vertices[] = {
        {Vector3(-0.5f, -0.5f, -0.5f)},
        {Vector3(-0.5f, 0.5f, -0.5f)},
        {Vector3(0.5f, 0.5f, -0.5f)},
        {Vector3(0.5f, -0.5f, -0.5f)},
        {Vector3(-0.5f, -0.5f, 0.5f)},
        {Vector3(-0.5f, 0.5f, 0.5f)},
        {Vector3(0.5f, 0.5f, 0.5f)},
        {Vector3(0.5f, -0.5f, 0.5f)}};

    const UINT vertexBufferSize = sizeof(vertices);

    // Create vertex buffer
    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC vertexBufDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBufferUpload));

    // Copy vertex data
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices;
    vertexData.RowPitch = vertexBufferSize;
    vertexData.SlicePitch = vertexBufferSize;

    UpdateSubresources(commandList, vertexBuffer.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

    // Transition vertex buffer to vertex buffer state
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
        vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    commandList->ResourceBarrier(1, &transition);

    // Indices
    UINT indices[] = {
        0, 1, 2, 0, 2, 3, // Front
        4, 6, 5, 4, 7, 6, // Back
        4, 5, 1, 4, 1, 0, // Left
        3, 2, 6, 3, 6, 7, // Right
        1, 5, 6, 1, 6, 2, // Top
        4, 0, 3, 4, 3, 7  // Bottom
    };

    const UINT indexBufferSize = sizeof(indices);

    // Create index buffer
    CD3DX12_RESOURCE_DESC indexBufDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    CD3DX12_HEAP_PROPERTIES defaultHeapForIndex(D3D12_HEAP_TYPE_DEFAULT);

    device->CreateCommittedResource(
        &defaultHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&indexBuffer));

    // Create upload heap for index buffer
    CD3DX12_HEAP_PROPERTIES uploadHeapForIndex(D3D12_HEAP_TYPE_UPLOAD);
    device->CreateCommittedResource(
        &uploadHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBufferUpload));

    // Copy index data
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = indices;
    indexData.RowPitch = indexBufferSize;
    indexData.SlicePitch = indexData.RowPitch;

    UpdateSubresources(commandList, indexBuffer.Get(), indexBufferUpload.Get(),
                       0, 0, 1, &indexData);

    // Create index buffer view
    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView.SizeInBytes = sizeof(indices);
}

void Cube::Draw(ID3D12GraphicsCommandList *commandList)
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
