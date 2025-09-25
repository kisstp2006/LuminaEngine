#pragma once
#include "imgui.h"
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
        
        struct FEntry
        {
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

        FEntry* GetOrCreateGroup(const FName& PathName);
        
    private:
        
        TVector<FEntry*>            Entries;
        THashMap<FName, FEntry*>    Images;
        TPair<FName, FEntry*>       SquareWhiteTexture;
    };
    
}
