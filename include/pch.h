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
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include <SimpleMath.h>

using Vector3 = DirectX::SimpleMath::Vector3;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")