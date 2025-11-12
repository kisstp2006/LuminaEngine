#include "RenderContext.h"
#include "RenderResource.h"
#include "RHIGlobals.h"
#include "Shader.h"

namespace Lumina
{
    void FShaderLibrary::CreateAndAddShader(FName Key, const FShaderHeader& Header, bool bReloadPipelines)
    {
        FRHIShaderRef Shader;
        
        switch (Header.Reflection.ShaderType)
        {
        case ERHIShaderType::None: break;
        case ERHIShaderType::Vertex:
            {
                Shader = GRenderContext->CreateVertexShader(Header);
            }
            break;
        case ERHIShaderType::Fragment:
            {
                Shader = GRenderContext->CreatePixelShader(Header);
            }
            break;
        case ERHIShaderType::Compute:
            {
                Shader = GRenderContext->CreateComputeShader(Header);
            }
            break;
        case ERHIShaderType::Geometry:
            {
                Shader = GRenderContext->CreateGeometryShader(Header);
            }
            break;
        }

        AddShader(Key, Shader);
        GRenderContext->OnShaderCompiled(Shader, false, bReloadPipelines);
    }

    void FShaderLibrary::AddShader(FName Key, FRHIShader* Shader)
    {
        FScopeLock Lock(Mutex);   
        Shaders.insert_or_assign(Key, Shader);
    }

    void FShaderLibrary::RemoveShader(FName Key)
    {
        FScopeLock Lock(Mutex);
        Shaders.erase(Key);
    }

    FRHIVertexShaderRef FShaderLibrary::GetVertexShader(FName Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIVertexShader>(Key);
    }

    FRHIPixelShaderRef FShaderLibrary::GetPixelShader(FName Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIPixelShader>(Key);
    }

    FRHIComputeShaderRef FShaderLibrary::GetComputeShader(FName Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIComputeShader>(Key);
    }

    FRHIGeometryShaderRef FShaderLibrary::GetGeometryShader(FName Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIGeometryShader>(Key);
    }

    FRHIShaderRef FShaderLibrary::GetShader(FName Key)
    {
        return Shaders.at(Key);
    }
}
