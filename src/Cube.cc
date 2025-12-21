#include "framework/Cube.h"

#include "framework/RenderTemplatesAPI.h"

void Cube::Initialize(ID3D12Device *dev, ID3D12GraphicsCommandList *commandList)
{
    device = dev;

    Vertex vertices[] = {
        {Vector4(-0.5f, -0.5f, -0.5f, 1.0f)},
        {Vector4(-0.5f, 0.5f, -0.5f, 1.0f)},
        {Vector4(0.5f, 0.5f, -0.5f, 1.0f)},
        {Vector4(0.5f, -0.5f, -0.5f, 1.0f)},
        {Vector4(-0.5f, -0.5f, 0.5f, 1.0f)},
        {Vector4(-0.5f, 0.5f, 0.5f, 1.0f)},
        {Vector4(0.5f, 0.5f, 0.5f, 1.0f)},
        {Vector4(0.5f, -0.5f, 0.5f, 1.0f)}};

    const UINT vertexBufferSize = sizeof(vertices);

    D3D12_HEAP_PROPERTIES defaultHeap = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC vertexBufDesc = GetBufferResourceDesc(vertexBufferSize);

    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(vertexBuffer.put()));

    D3D12_HEAP_PROPERTIES uploadHeap = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(vertexBufferUpload.put()));

    UINT8 *pVertexData;
    D3D12_RANGE readRange = {0, 0};
    vertexBufferUpload->Map(0, &readRange, reinterpret_cast<void **>(&pVertexData));
    memcpy(pVertexData, vertices, vertexBufferSize);
    vertexBufferUpload->Unmap(0, nullptr);

    commandList->CopyBufferRegion(vertexBuffer.get(), 0, vertexBufferUpload.get(), 0, vertexBufferSize);

    D3D12_RESOURCE_BARRIER transition = GetTransitionCopyToVertex(vertexBuffer.get());
    commandList->ResourceBarrier(1, &transition);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = vertexBufferSize;
    vertexBufferView.StrideInBytes = sizeof(Vertex);

    UINT indices[] = {
        0, 1, 2, 0, 2, 3, // Front
        4, 6, 5, 4, 7, 6, // Back
        4, 5, 1, 4, 1, 0, // Left
        3, 2, 6, 3, 6, 7, // Right
        1, 5, 6, 1, 6, 2, // Top
        4, 0, 3, 4, 3, 7  // Bottom
    };

    const UINT indexBufferSize = sizeof(indices);

    D3D12_RESOURCE_DESC indexBufDesc = GetBufferResourceDesc(indexBufferSize);
    D3D12_HEAP_PROPERTIES defaultHeapForIndex = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    device->CreateCommittedResource(
        &defaultHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(indexBuffer.put()));

    D3D12_HEAP_PROPERTIES uploadHeapForIndex = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

    device->CreateCommittedResource(
        &uploadHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(indexBufferUpload.put()));

    UINT8 *pIndexData;
    indexBufferUpload->Map(0, &readRange, reinterpret_cast<void **>(&pIndexData));
    memcpy(pIndexData, indices, indexBufferSize);
    indexBufferUpload->Unmap(0, nullptr);

    commandList->CopyBufferRegion(indexBuffer.get(), 0, indexBufferUpload.get(), 0, indexBufferSize);

    D3D12_RESOURCE_BARRIER transitionIndex = GetTransitionCopyToIndex(indexBuffer.get());
    commandList->ResourceBarrier(1, &transitionIndex);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView.SizeInBytes = sizeof(indices);
}

void Cube::CreateConstBuffer(ID3D12Device *device, DescriptorAllocator &alloc, UINT heapIndex)
{
    D3D12_HEAP_PROPERTIES heapProps = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC cbDesc = GetBufferResourceDesc((sizeof(CubeConstBuffer) + 255) & ~255);

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_constBuffer.put()));

    m_constBufferAddress = m_constBuffer->GetGPUVirtualAddress();

    // CBV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_constBufferAddress;
    cbvDesc.SizeInBytes = (sizeof(CubeConstBuffer) + 255) & ~255;

    // allocate descriptor slot from allocator and create CBV there
    UINT idx = alloc.Alloc();
    m_constBufferView = alloc.GetCpuHandle(idx);
    device->CreateConstantBufferView(&cbvDesc, m_constBufferView);

    m_constBufferGPUHandle = alloc.GetGpuHandle(idx);
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
    // Use GPU descriptor handle
    commandList->SetGraphicsRootDescriptorTable(1, m_constBufferGPUHandle);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
