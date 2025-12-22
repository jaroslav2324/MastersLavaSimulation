#include "framework/UploadHelpers.h"
#include "framework/RenderTemplatesAPI.h"
#include "framework/RenderSubsystem.h"
#include <wrl/client.h>

namespace UploadHelpers
{
    winrt::com_ptr<ID3D12Resource> CreateUploadBuffer(ID3D12Device *device, UINT64 size)
    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

        winrt::com_ptr<ID3D12Resource> upload;
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(upload.put())));

        return upload;
    }

    void CopyBufferToResource(
        ID3D12GraphicsCommandList *cmdList,
        ID3D12Resource *uploadResource,
        ID3D12Resource *dstResource,
        UINT64 size,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after)
    {
        // transition dst -> COPY_DEST
        CD3DX12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            dstResource,
            before,
            D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList->ResourceBarrier(1, &toCopy);

        // copy
        cmdList->CopyBufferRegion(dstResource, 0, uploadResource, 0, size);

        // transition COPY_DEST -> after
        CD3DX12_RESOURCE_BARRIER toAfter = CD3DX12_RESOURCE_BARRIER::Transition(
            dstResource,
            D3D12_RESOURCE_STATE_COPY_DEST,
            after);
        cmdList->ResourceBarrier(1, &toAfter);
    }
}
