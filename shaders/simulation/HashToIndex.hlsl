StructuredBuffer<uint> hashes : register(t0);

// размер буферов - количество клеток в кубе
RWStructuredBuffer<int> cellStart : register(u0);
RWStructuredBuffer<int> cellEnd   : register(u1);

cbuffer Params : register(b0)
{
    uint NumParticles;
    uint NumCells;
};

[numthreads(256, 1, 1)]
void CS_FindCellRanges(uint3 DTid : SV_DispatchThreadID)
{
    uint i = DTid.x;
    if (i >= NumParticles)
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
    if (i == NumParticles - 1)
    {
        cellEnd[h] = NumParticles;
    }
}