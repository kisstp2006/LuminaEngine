#pragma once
#include "imgui.h"
#include "Containers/SparseArray.h"
#include "Core/Threading/Thread.h"
#include "Memory/Allocators/Allocator.h"
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    class FUITextureCache
    {
    public:
        
        enum class ETextureState : uint8
        {
            Empty,
            Loading,
            Ready,
        };
        
        struct LUMINA_API FEntry
        {
            FName Name;
            uint64 Index;
            std::atomic_uint64_t LastUseFrame{0};
            std::atomic<ETextureState> State = ETextureState::Empty;
            FRHIImageRef RHIImage;
            ImTextureRef ImTexture;
        };
        

        FUITextureCache();
        ~FUITextureCache();

        LUMINA_API static FUITextureCache& Get();
        
        LUMINA_API FRHIImageRef GetImage(const FName& Path);
        LUMINA_API ImTextureRef GetImTexture(const FName& Path);
        
        
        void StartFrame();
        void EndFrame();

        bool HasImagesPendingLoad() const;
    
    private:

        bool HasImagesPendingLoad_Internal() const;
        FEntry* GetOrCreateGroup_Internal(const FName& PathName);
        
    private:

        mutable FMutex                  Mutex;

        THashMap<FName, FEntry*>        Images;
        
        TPair<FName, FEntry*>           SquareWhiteTexture;
    };
    
}
