#pragma once

#include "inc.h"

class ShaderCompiler
{
public:
    struct CompileResult
    {
        Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        bool success;
    };

    // Compile HLSL shader from file
    static CompileResult CompileFromFile(
        const std::wstring &filename,
        const std::string &entryPoint,
        const std::string &target,
        const std::vector<D3D_SHADER_MACRO> &defines = {});
};
