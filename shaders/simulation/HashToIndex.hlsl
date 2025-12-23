#include "CommonData.hlsl"

StructuredBuffer<uint> hashes : register(t3);

RWStructuredBuffer<int> cellStart : register(u5);
RWStructuredBuffer<int> cellEnd   : register(u6);

[numthreads(256, 1, 1)]
void CS_FindCellRanges(uint3 DTid : SV_DispatchThreadID)
{
    uint i = DTid.x;
    if (i >= numParticles)
        return;

    uint h = hashes[i];

    // Если мы первый элемент или хэш изменился — начало новой группы
    if (i == 0 || h != hashes[i - 1])
    {
        cellStart[h] = i;

        // Если не первый — надо закрыть предыдущий h
        if (i > 0)
        {
            uint prevH = hashes[i - 1];
            cellEnd[prevH] = i;
        }
    }

    // Последний элемент массива закрывает свою группу
    if (i == numParticles - 1)
    {
        cellEnd[h] = numParticles;
    }
}