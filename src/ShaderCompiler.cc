#include "framework/ShaderCompiler.h"
#include <d3dcompiler.h>
#include <fstream>

// TODO: dxc can be used
ShaderCompiler::CompileResult ShaderCompiler::CompileFromFile(
    const std::wstring &filename,
    const std::string &entryPoint,
    const std::string &target,
    const std::vector<D3D_SHADER_MACRO> &defines)
{
    CompileResult result = {};
    //     UINT compileFlags = 0;
    // #if defined(_DEBUG)
    //     compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    // #endif
    UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

    const D3D_SHADER_MACRO *pDefines = defines.empty() ? nullptr : defines.data();

    HRESULT hr = D3DCompileFromFile(
        filename.c_str(),
        pDefines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(),
        target.c_str(),
        compileFlags,
        0,
        result.bytecode.put(),
        result.error.put());

    result.success = SUCCEEDED(hr);
    return result;
}
