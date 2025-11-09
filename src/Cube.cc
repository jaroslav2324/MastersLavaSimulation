#include "Cube.h"

#include "RenderTemplatesAPI.h"

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

    D3D12_HEAP_PROPERTIES defaultHeap = GetDefaultHeapProperties();
    D3D12_RESOURCE_DESC vertexBufDesc = GetBufferResourceDesc(vertexBufferSize);

    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));

    D3D12_HEAP_PROPERTIES uploadHeap = GetUploadHeapProperties();

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBufferUpload));

    UINT8 *pVertexData;
    D3D12_RANGE readRange = {0, 0};
    vertexBufferUpload->Map(0, &readRange, reinterpret_cast<void **>(&pVertexData));
    memcpy(pVertexData, vertices, vertexBufferSize);
    vertexBufferUpload->Unmap(0, nullptr);

    commandList->CopyBufferRegion(vertexBuffer.Get(), 0, vertexBufferUpload.Get(), 0, vertexBufferSize);

    D3D12_RESOURCE_BARRIER transition = GetTransitionCopyToVertex(vertexBuffer.Get());
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
    D3D12_HEAP_PROPERTIES defaultHeapForIndex = GetDefaultHeapProperties();

    device->CreateCommittedResource(
        &defaultHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&indexBuffer));

    D3D12_HEAP_PROPERTIES uploadHeapForIndex = GetUploadHeapProperties();

    device->CreateCommittedResource(
        &uploadHeapForIndex,
        D3D12_HEAP_FLAG_NONE,
        &indexBufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBufferUpload));

    UINT8 *pIndexData;
    indexBufferUpload->Map(0, &readRange, reinterpret_cast<void **>(&pIndexData));
    memcpy(pIndexData, indices, indexBufferSize);
    indexBufferUpload->Unmap(0, nullptr);

    commandList->CopyBufferRegion(indexBuffer.Get(), 0, indexBufferUpload.Get(), 0, indexBufferSize);

    D3D12_RESOURCE_BARRIER transitionIndex = GetTransitionCopyToIndex(indexBuffer.Get());
    commandList->ResourceBarrier(1, &transitionIndex);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView.SizeInBytes = sizeof(indices);
}

void Cube::CreateConstBuffer(ID3D12Device *device, ID3D12DescriptorHeap *heap, UINT heapIndex)
{
    D3D12_HEAP_PROPERTIES heapProps = GetUploadHeapProperties();
    D3D12_RESOURCE_DESC cbDesc = GetBufferResourceDesc((sizeof(CubeConstBuffer) + 255) & ~255);

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

    // compute CPU descriptor handle for this slot
    m_constBufferView = heap->GetCPUDescriptorHandleForHeapStart();
    SIZE_T increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_constBufferView.ptr += static_cast<SIZE_T>(heapIndex) * increment;
    device->CreateConstantBufferView(&cbvDesc, m_constBufferView);

    // compute GPU descriptor handle for this slot (for SetGraphicsRootDescriptorTable)
    m_constBufferGPUHandle = heap->GetGPUDescriptorHandleForHeapStart();
    m_constBufferGPUHandle.ptr += static_cast<SIZE_T>(heapIndex) * increment;
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
