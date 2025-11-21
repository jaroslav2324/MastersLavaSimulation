// =============================================================
// Spatial Hash Compute Shader for PBF/FLIP/SPH
// Input: particle positions
// Output: hash[i], index[i]
// =============================================================

cbuffer SimParams : register(b0)
{
    float3 worldMin;      // минимальная точка симуляции (если используется ограниченный мир)
    float cellSize;       // размер grid cell = kernel radius
    uint numParticles;
    uint hashTableMask;   // (hashTableSize - 1), если size = степень двойки
};

// Input
StructuredBuffer<float3> positionBuffer : register(t0);

// Output
RWStructuredBuffer<uint> hashBuffer  : register(u0);
RWStructuredBuffer<uint> indexBuffer : register(u1);

[numthreads(256, 1, 1)]
void CS_HashParticles(uint id : SV_DispatchThreadID)
{
    if (id >= numParticles) return;

    float3 pos = positionBuffer[id];

    //------------------------------------------------------------
    // Convert world position → grid cell
    //------------------------------------------------------------
    int3 cell = int3(floor((pos - worldMin) / cellSize));

    //------------------------------------------------------------
    // --- Hash function variant 1:
    //     Spatial hash for infinite grid
    //------------------------------------------------------------
    const uint p1 = 73856093;
    const uint p2 = 19349663;
    const uint p3 = 83492791;

    uint h1 = (uint(cell.x) * p1) ^ (uint(cell.y) * p2) ^ (uint(cell.z) * p3);

    // bring into table range
    uint hashValue = h1 & hashTableMask;

    //------------------------------------------------------------
    // --- Optional second hash 
    // uint h2 = reversebits(h1) * 1664525u; // alternate hash
    //------------------------------------------------------------

    hashBuffer[id]  = hashValue;
    indexBuffer[id] = id;
}