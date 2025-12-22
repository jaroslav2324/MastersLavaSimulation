#pragma once

#include "pch.h"
#include <wrl/client.h>

namespace UploadHelpers
{
    winrt::com_ptr<ID3D12Resource> CreateUploadBuffer(ID3D12Device *device, UINT64 size);

    // Copy from an upload resource to dst resource and transition dst from 'before' to 'after'
    void CopyBufferToResource(
        ID3D12GraphicsCommandList *cmdList,
        ID3D12Resource *uploadResource,
        ID3D12Resource *dstResource,
        UINT64 size,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after);
}
