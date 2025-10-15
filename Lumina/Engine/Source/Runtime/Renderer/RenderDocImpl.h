#pragma once
#include "renderdoc_app.h"
#include "Containers/String.h"
#include "Module/API.h"

namespace Lumina
{
    class FRenderDoc
    {
    public:

        FRenderDoc();

        LUMINA_API static FRenderDoc& Get();

        LUMINA_API void StartFrameCapture() const;
        LUMINA_API void EndFrameCapture() const;
        LUMINA_API void TriggerCapture() const;
        LUMINA_API const char* GetCaptureFilePath() const;
        LUMINA_API void TryOpenRenderDocUI();
        
    private:

        FString RenderDocExePath;
        RENDERDOC_API_1_1_2* RenderDocAPI = nullptr;
    };
}
