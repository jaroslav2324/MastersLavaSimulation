// somewhat like pch
#pragma once

#define WIN32_LEAN_AND_MEAN
#define STRICT
#define NOMINMAX

#include <wil/resource.h>
#include <winrt/base.h>

#include <directx/d3dx12.h>
#include <directx/d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>

#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <random>
#include <cstring>
#include <algorithm>
#include <cassert>

#include <SimpleMath.h>

using Vector2 = DirectX::SimpleMath::Vector2;
using Vector3 = DirectX::SimpleMath::Vector3;
using Vector4 = DirectX::SimpleMath::Vector4;
using Matrix = DirectX::SimpleMath::Matrix;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

inline int32_t Align256(int32_t size)
{
    return (size + 255) & ~255;
}