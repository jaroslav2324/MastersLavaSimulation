#include "CommonData.hlsl"

StructuredBuffer<uint> hashes : register(t3);

RWStructuredBuffer<int> cellStart : register(u5);
RWStructuredBuffer<int> cellEnd   : register(u6);

[numthreads(256, 1, 1)]
void CS_FindCellRanges(uint gid : SV_DispatchThreadID)
{
    if (gid >= numParticles) return;
    uint i = gid;

    uint h = hashes[i];

    if (i == 0 || h != hashes[i - 1])
    {
        cellStart[h] = i;

        if (i > 0)
        {
            uint prevH = hashes[i - 1];
            cellEnd[prevH] = i;
        }
    }

    if (i == numParticles - 1)
    {
        cellEnd[h] = numParticles;
    }
}
