cbuffer Params : register(b0)
{
    uint g_Shift;        // текущий сдвиг (бит)
    uint NumElements;    // общее число элементов для сортировки
    uint NumCounts;      // = NumBlocks * 2
    uint totalZeros;    // общее число нулей (для вычисления индекса единиц)
};

struct Element
{
    uint key;
    uint particleIndex;  
};

RWStructuredBuffer<Element> Input  : register(u0);
RWStructuredBuffer<Element> Output : register(u1);

RWStructuredBuffer<uint> BlockCounts : register(u2);  
RWStructuredBuffer<uint> Prefix      : register(u3); 

[numthreads(256, 1, 1)]
void CountKernel(uint3 DTid : SV_DispatchThreadID,
                 uint3 GTid : SV_GroupThreadID,
                 uint3 GId  : SV_GroupID)
{
    uint globalIndex = DTid.x;
    uint groupIndex  = GId.x;
    uint localId     = GTid.x;

    if (globalIndex >= NumElements)
        return;

    // groupshared counters (one per group)
    groupshared uint s_zeros;
    groupshared uint s_ones;

    if (localId == 0)
    {
        s_zeros = 0;
        s_ones  = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    Element e = Input[globalIndex];
    uint bit = (e.key >> g_Shift) & 1;

    if (bit == 0)
        InterlockedAdd(s_zeros, 1);
    else
        InterlockedAdd(s_ones, 1);

    GroupMemoryBarrierWithGroupSync();

    if (localId == 0)
    {
        uint base = groupIndex * 2;
        BlockCounts[base + 0] = s_zeros;
        BlockCounts[base + 1] = s_ones;
    }
}

// [numthreads(256,1,1)]
// void PrefixSum(uint3 DTid : SV_DispatchThreadID)
// {
//     uint index = DTid.x;
//     uint x = BlockCounts[index];

//     groupshared uint temp[512]; 

//     temp[index] = x;
//     GroupMemoryBarrierWithGroupSync();

//     for (uint offset = 1; offset < NumCounts; offset <<= 1)
//     {
//         uint val = temp[index];
//         if (index >= offset)
//             val += temp[index - offset];

//         GroupMemoryBarrierWithGroupSync();
//         temp[index] = val;
//         GroupMemoryBarrierWithGroupSync();
//     }

//     Prefix[index] = (index == 0) ? 0 : temp[index - 1];
// }

[numthreads(256,1,1)]
void ScatterKernel(uint3 DTid : SV_DispatchThreadID,
                   uint3 GTid : SV_GroupThreadID,
                   uint3 GId  : SV_GroupID)
{
    uint globalIndex = DTid.x;
    uint localId     = GTid.x;
    uint groupIndex  = GId.x;

    if (globalIndex >= NumElements)
        return;

    Element e = Input[globalIndex];
    uint bit = (e.key >> g_Shift) & 1;

    // локальные массивы длины 256
    groupshared uint localZeros[256];
    groupshared uint localOnes[256];

    // помечаем локально
    localZeros[localId] = (bit == 0) ? 1u : 0u;
    localOnes [localId] = (bit == 1) ? 1u : 0u;

    GroupMemoryBarrierWithGroupSync();

    // in-place Hillis-Steele scan (локальный эксклюзив/инклюзив — мы извлечём индекс как value-1)
    for (uint offset = 1; offset < 256; offset <<= 1)
    {
        uint v0 = 0;
        uint v1 = 0;
        if (localId >= offset)
        {
            v0 = localZeros[localId - offset];
            v1 = localOnes [localId - offset];
        }
        GroupMemoryBarrierWithGroupSync();
        localZeros[localId] += v0;
        localOnes [localId] += v1;
        GroupMemoryBarrierWithGroupSync();
    }

    uint localZeroIndex = localZeros[localId] - 1; // zero-based index among zeros in block
    uint localOneIndex  = localOnes [localId] - 1; // zero-based among ones in block

    uint base = groupIndex * 2;
    uint globalZerosBefore = Prefix[base + 0]; // exclusive prefix for zeros
    uint globalOnesBefore  = Prefix[base + 1]; // exclusive prefix for ones

    uint newIndex =
        (bit == 0)
        ? (globalZerosBefore + localZeroIndex)
        : (totalZeros + globalOnesBefore + localOneIndex);

    // bounds check (Output size must be >= NumElements)
    Output[newIndex] = e;
}