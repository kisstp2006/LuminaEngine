#include "RenderContext.h"
#include "RenderResource.h"
#include "RHIGlobals.h"

namespace Lumina
{
    void FShaderLibrary::AddShader(FRHIShader* Shader)
    {
        FScopeLock Lock(Mutex);
        
        Shaders.insert_or_assign(Shader->GetKey(), Shader);
    }

    void FShaderLibrary::RemoveShader(FName Key)
    {
        FScopeLock Lock(Mutex);
        
        auto It = Shaders.find(Key);
        Assert(It != Shaders.end())
        
        Shaders.erase(It);
    }

    FRHIVertexShaderRef FShaderLibrary::GetVertexShader(const FName& Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIVertexShader>(Key);
    }

    FRHIPixelShaderRef FShaderLibrary::GetPixelShader(const FName& Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIPixelShader>(Key);
    }

    FRHIComputeShaderRef FShaderLibrary::GetComputeShader(const FName& Key)
    {
        return GRenderContext->GetShaderLibrary()->GetShader<FRHIComputeShader>(Key);
    }

    FRHIShaderRef FShaderLibrary::GetShader(const FName& Key)
    {
        LUMINA_PROFILE_SCOPE();
        
        auto It = Shaders.find(Key);
        if (It != Shaders.end())
        {
            return It->second;
        }

        LOG_WARN("Shader with key [{}] not found.", Key);
        return nullptr;
    }
}
