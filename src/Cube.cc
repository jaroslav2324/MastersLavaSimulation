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
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC vertexBufDesc = {};
    vertexBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexBufDesc.Alignment = 0;
    vertexBufDesc.Width = vertexBufferSize;
    vertexBufDesc.Height = 1;
    vertexBufDesc.DepthOrArraySize = 1;
    vertexBufDesc.MipLevels = 1;
    vertexBufDesc.Format = DXGI_FORMAT_UNKNOWN;
    vertexBufDesc.SampleDesc.Count = 1;
    vertexBufDesc.SampleDesc.Quality = 0;
    vertexBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    vertexBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBufferUpload));

    // Manual copy vertex data
    UINT8 *pVertexData;
    D3D12_RANGE readRange = {0, 0};
    vertexBufferUpload->Map(0, &readRange, reinterpret_cast<void **>(&pVertexData));
    memcpy(pVertexData, vertices, vertexBufferSize);
    vertexBufferUpload->Unmap(0, nullptr);

    // Copy from upload heap to default heap
    commandList->CopyBufferRegion(vertexBuffer.Get(), 0, vertexBufferUpload.Get(), 0, vertexBufferSize);

    // Transition vertex buffer to vertex buffer state
    D3D12_RESOURCE_BARRIER transition = {};
    transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transition.Transition.pResource = vertexBuffer.Get();
    transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    transition.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
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
    D3D12_RESOURCE_DESC indexBufDesc = {};
    indexBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    indexBufDesc.Alignment = 0;
    indexBufDesc.Width = indexBufferSize;
    indexBufDesc.Height = 1;
    indexBufDesc.DepthOrArraySize = 1;
    indexBufDesc.MipLevels = 1;
    indexBufDesc.Format = DXGI_FORMAT_UNKNOWN;
    indexBufDesc.SampleDesc.Count = 1;
    indexBufDesc.SampleDesc.Quality = 0;
    indexBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    indexBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapForIndex = {};
    defaultHeapForIndex.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeapForIndex.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeapForIndex.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeapForIndex.CreationNodeMask = 1;
    defaultHeapForIndex.VisibleNodeMask = 1;

    device->CreateCommittedResource(
        &defaultHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&indexBuffer));

    D3D12_HEAP_PROPERTIES uploadHeapForIndex = {};
    uploadHeapForIndex.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapForIndex.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapForIndex.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeapForIndex.CreationNodeMask = 1;
    uploadHeapForIndex.VisibleNodeMask = 1;

    device->CreateCommittedResource(
        &uploadHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBufferUpload));

    // Manual copy index data
    UINT8 *pIndexData;
    indexBufferUpload->Map(0, &readRange, reinterpret_cast<void **>(&pIndexData));
    memcpy(pIndexData, indices, indexBufferSize);
    indexBufferUpload->Unmap(0, nullptr);

    // Copy from upload heap to default heap
    commandList->CopyBufferRegion(indexBuffer.Get(), 0, indexBufferUpload.Get(), 0, indexBufferSize);

    // Create index buffer view
    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView.SizeInBytes = sizeof(indices);
}

void Cube::CreateConstBuffer(ID3D12Device *device, ID3D12DescriptorHeap *heap, UINT heapIndex)
{
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Alignment = 0;
    cbDesc.Width = (sizeof(CubeConstBuffer) + 255) & ~255;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.SampleDesc.Quality = 0;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constBuffer));

    m_constBufferAddress = m_constBuffer->GetGPUVirtualAddress();

    // CBV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_constBufferAddress;
    cbvDesc.SizeInBytes = (sizeof(CubeConstBuffer) + 255) & ~255;
    m_constBufferView = heap->GetCPUDescriptorHandleForHeapStart();
    m_constBufferView.ptr += heapIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateConstantBufferView(&cbvDesc, m_constBufferView);
}

void Cube::UpdateConstBuffer()
{
    m_constBufferData.model = transform.GetModelMatrix();
    UINT8 *pData;
    D3D12_RANGE readRange = {0, 0};
    m_constBuffer->Map(0, &readRange, reinterpret_cast<void **>(&pData));
    memcpy(pData, &m_constBufferData, sizeof(CubeConstBuffer));
    m_constBuffer->Unmap(0, nullptr);
}

void Cube::Draw(ID3D12GraphicsCommandList *commandList)
{
    UpdateConstBuffer();
    // Set descriptor heap and root descriptor table for this cube's CBV (assume root parameter 1 for model)
    commandList->SetGraphicsRootDescriptorTable(1, m_constBufferView);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
