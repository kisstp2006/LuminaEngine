#include "RenderContext.h"
#include "RenderResource.h"
#include "RHIGlobals.h"

namespace Lumina
{
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

    FRHIShaderRef FShaderLibrary::GetShader(FName Key)
    {
        return Shaders.at(Key);
    }
}
