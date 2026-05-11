#include "framework/SceneSetup.h"
#include <random>

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

static std::vector<Vector3> GenerateDenseBottomWithSphere(UINT numParticles)
{
    std::vector<Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> u(0.0f, 5.0f);

    const UINT sphereCount = static_cast<UINT>(std::round(numParticles * 0.18f));
    const UINT bottomCount = numParticles - sphereCount;

    for (UINT i = 0; i < bottomCount; ++i)
        out.emplace_back(u(rng), u(rng) / 5.0f, u(rng));

    Vector3 center(2.5f, 2.82f, 2.5f);
    const float radius = 1.0f;
    std::uniform_real_distribution<float> uSphere(-radius, radius);
    while (out.size() < numParticles)
    {
        float rx = uSphere(rng), ry = uSphere(rng), rz = uSphere(rng);
        if (rx * rx + ry * ry + rz * rz <= radius * radius)
            out.push_back(center + Vector3(rx, ry, rz));
    }
    return out;
}

static std::vector<Vector3> GenerateDamBreak(UINT numParticles)
{
    std::vector<Vector3> out;
    out.reserve(numParticles);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    float thickness = 2.0f, height = 5.0f, width = 5.0f;
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

static void GenerateTwoSpheres(UINT numParticles,
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

    const float radius = 0.85f;
    // Spheres rest on the ground (y_center = radius), separated in x.
    Vector3 hotCenter(2.5f, radius, 2.5f);
    Vector3 coldCenter(5.0f, radius, 2.5f);

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
                                             std::vector<float> &outTemps)
{
    const float hotHeight = 1.8f;
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
                           std::vector<float> &outTemperatures)
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
        outPositions = GenerateDenseBottomWithSphere(numParticles);
        GenerateTemperaturesForPositions(outPositions, outTemperatures);
        break;

    case SceneType::DamBreak:
        outPositions = GenerateDamBreak(numParticles);
        GenerateDamBreakTemperatures(outPositions, outTemperatures);
        break;

    case SceneType::TwoSpheres:
        GenerateTwoSpheres(numParticles, outPositions, outTemperatures);
        break;
    }
}
