#include "framework/SceneSetup.h"
#include <random>

// Reference particle count for scene scaling.
// TODO: calculate from desired density?
static constexpr UINT kSceneRef = 4096;
static constexpr float kGridMargin = 0.95f; // keep particles this fraction inside the grid

static float SceneScale(UINT numParticles)
{
    return std::cbrt(static_cast<float>(numParticles) / static_cast<float>(kSceneRef));
}

// Analytical max extent (from origin) of each scene at unit scale, on any axis.
// Used to clamp scale so that all generated particles fit inside the grid.
static constexpr float kExtentTwoSpheres = 5.85f;           // coldCenter.x + radius = 5.0 + 0.85
static constexpr float kExtentDenseBottomWithSphere = 5.0f; // flat region [0,5] in x/z
static constexpr float kExtentDamBreak = 5.0f;              // height/width = 5

static float ClampedSceneScale(UINT numParticles, float baseExtent, float gridWorldSize)
{
    float scale = SceneScale(numParticles);
    if (gridWorldSize > 0.0f)
        scale = std::min(scale, gridWorldSize * kGridMargin / baseExtent);
    return scale;
}

// ---------------------------------------------------------------------------

static std::vector<Vector3> GenerateUniformGrid(UINT numParticles)
{
    std::vector<Vector3> out;
    out.reserve(numParticles);
    int n = static_cast<int>(std::ceil(std::cbrt((double)numParticles)));
    for (int z = 0; z < n && out.size() < numParticles; ++z)
        for (int y = 0; y < n && out.size() < numParticles; ++y)
            for (int x = 0; x < n && out.size() < numParticles; ++x)
                out.emplace_back((x + 0.5f) / n, (y + 0.5f) / n, (z + 0.5f) / n);
    return out;
}

static std::vector<Vector3> GenerateDenseRandom(UINT numParticles, unsigned seed = 1337)
{
    std::vector<Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    int m = static_cast<int>(std::ceil(std::cbrt((double)numParticles)));
    for (int z = 0; z < m && out.size() < numParticles; ++z)
        for (int y = 0; y < m && out.size() < numParticles; ++y)
            for (int x = 0; x < m && out.size() < numParticles; ++x)
                out.emplace_back((x + u(rng)) / m, (y + u(rng)) / m, (z + u(rng)) / m);
    return out;
}

static std::vector<Vector3> GenerateDenseBottomWithSphere(UINT numParticles, float scale)
{
    std::vector<Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> u(0.0f, 5.0f * scale);

    const UINT sphereCount = static_cast<UINT>(std::round(numParticles * 0.18f));
    const UINT bottomCount = numParticles - sphereCount;

    for (UINT i = 0; i < bottomCount; ++i)
        out.emplace_back(u(rng), u(rng) / 5.0f, u(rng));

    Vector3 center(2.5f * scale, 2.82f * scale, 2.5f * scale);
    const float radius = 1.0f * scale;
    std::uniform_real_distribution<float> uSphere(-radius, radius);
    while (out.size() < numParticles)
    {
        float rx = uSphere(rng), ry = uSphere(rng), rz = uSphere(rng);
        if (rx * rx + ry * ry + rz * rz <= radius * radius)
            out.push_back(center + Vector3(rx, ry, rz));
    }
    return out;
}

static std::vector<Vector3> GenerateDamBreak(UINT numParticles, float scale)
{
    std::vector<Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    float thickness = 2.0f * scale, height = 5.0f * scale, width = 5.0f * scale;
    double cubeRoot = std::cbrt(numParticles / (thickness * height * width));
    int nx = std::max(1, (int)std::round(thickness * cubeRoot));
    int ny = std::max(1, (int)std::round(height * cubeRoot));
    int nz = std::max(1, (int)std::round(width * cubeRoot));

    for (int z = 0; z < nz && out.size() < numParticles; ++z)
        for (int y = 0; y < ny && out.size() < numParticles; ++y)
            for (int x = 0; x < nx && out.size() < numParticles; ++x)
                out.emplace_back((x + u(rng)) / nx * thickness,
                                 (y + u(rng)) / ny * height,
                                 (z + u(rng)) / nz * width);
    return out;
}

// Fills a sphere with rejection sampling; appends to `out` until `count` particles added.
static void FillSphere(std::vector<Vector3> &out, UINT count,
                       Vector3 center, float radius, std::mt19937 &rng)
{
    std::uniform_real_distribution<float> u(-radius, radius);
    UINT added = 0;
    while (added < count)
    {
        float rx = u(rng), ry = u(rng), rz = u(rng);
        if (rx * rx + ry * ry + rz * rz <= radius * radius)
        {
            out.push_back(center + Vector3(rx, ry, rz));
            ++added;
        }
    }
}

static void GenerateTwoSpheres(UINT numParticles, float scale,
                               std::vector<Vector3> &outPositions,
                               std::vector<float> &outTemperatures)
{
    outPositions.clear();
    outPositions.reserve(numParticles);
    outTemperatures.clear();
    outTemperatures.reserve(numParticles);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> hotRange(1300.0f, 1500.0f);
    std::uniform_real_distribution<float> coldRange(600.0f, 800.0f);

    const float radius = 0.85f * scale;
    Vector3 hotCenter(2.5f * scale, radius, 2.5f * scale);
    Vector3 coldCenter(5.0f * scale, radius, 2.5f * scale);

    UINT hotCount = numParticles / 2;
    UINT coldCount = numParticles - hotCount;

    FillSphere(outPositions, hotCount, hotCenter, radius, rng);
    for (UINT i = 0; i < hotCount; ++i)
        outTemperatures.push_back(hotRange(rng));

    FillSphere(outPositions, coldCount, coldCenter, radius, rng);
    for (UINT i = 0; i < coldCount; ++i)
        outTemperatures.push_back(coldRange(rng));
}

static void GenerateTemperaturesForPositions(const std::vector<Vector3> &positions,
                                             std::vector<float> &outTemps,
                                             float hotHeight = 1.8f)
{
    outTemps.clear();
    outTemps.reserve(positions.size());
    std::mt19937 rng(424242);
    std::uniform_real_distribution<float> coldRange(700.0f, 900.0f);
    std::uniform_real_distribution<float> hotRange(1200.0f, 1400.0f);
    for (const auto &p : positions)
        outTemps.push_back(p.y >= hotHeight ? hotRange(rng) : coldRange(rng));
}

static void GenerateDamBreakTemperatures(const std::vector<Vector3> &positions,
                                         std::vector<float> &outTemps)
{
    outTemps.clear();
    outTemps.reserve(positions.size());
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    for (size_t i = 0; i < positions.size(); ++i)
        outTemps.push_back(900.0f + 100.0f * (u(rng) - 0.5f));
}

void SceneSetup::LoadScene(SceneType scene, UINT numParticles,
                           std::vector<Vector3> &outPositions,
                           std::vector<float> &outTemperatures,
                           float gridWorldSize)
{
    switch (scene)
    {
    case SceneType::UniformGrid:
        outPositions = GenerateUniformGrid(numParticles);
        GenerateTemperaturesForPositions(outPositions, outTemperatures);
        break;

    case SceneType::DenseRandom:
        outPositions = GenerateDenseRandom(numParticles);
        GenerateTemperaturesForPositions(outPositions, outTemperatures);
        break;

    case SceneType::DenseBottomWithSphere:
    {
        float scale = ClampedSceneScale(numParticles, kExtentDenseBottomWithSphere, gridWorldSize);
        outPositions = GenerateDenseBottomWithSphere(numParticles, scale);
        GenerateTemperaturesForPositions(outPositions, outTemperatures, 1.8f * scale);
        break;
    }

    case SceneType::DamBreak:
    {
        float scale = ClampedSceneScale(numParticles, kExtentDamBreak, gridWorldSize);
        outPositions = GenerateDamBreak(numParticles, scale);
        GenerateDamBreakTemperatures(outPositions, outTemperatures);
        break;
    }

    case SceneType::TwoSpheres:
    {
        float scale = ClampedSceneScale(numParticles, kExtentTwoSpheres, gridWorldSize);
        GenerateTwoSpheres(numParticles, scale, outPositions, outTemperatures);
        break;
    }
    }
}
