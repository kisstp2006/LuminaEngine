#pragma once

#include "RenderTypes.h"
#include "Memory/Allocators/Allocator.h"
#include "Types/BitFlags.h"


namespace Lumina
{
    struct FTextureSubresourceSet;
    struct FRHIImageDesc;
    struct FRHIBufferDesc;
}

namespace Lumina
{
    namespace VkStateTracking
    {
        bool VerifyPermanentResourceState(EResourceStates PermanentState, EResourceStates RequiredState, bool bIsTexture, FStringView DebugName);
    }

    
    struct FBufferStateExtension
    {
        const FRHIBufferDesc& DescRef;
        EResourceStates PermanentState = EResourceStates::Unknown;

        explicit FBufferStateExtension(const FRHIBufferDesc& desc)
            : DescRef(desc)
        { }
    };

    struct FTextureStateExtension
    {
        explicit FTextureStateExtension(const FRHIImageDesc& desc)
            : DescRef(desc)
        { }
        
        const FRHIImageDesc& DescRef;
        EResourceStates PermanentState = EResourceStates::Unknown;
        bool stateInitialized = false;
        bool isSamplerFeedback = false;

    };

    struct FTextureState
    {
        TFixedVector<EResourceStates, 1> SubresourceStates;
        EResourceStates State = EResourceStates::Unknown;
        bool bEnableUavBarriers = true;
        bool bFirstUavBarrierPlaced = false;
        bool bPermanentTransition = false;
    };

    struct FBufferState
    {
        EResourceStates state = EResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
        bool permanentTransition = false;
    };

    struct FTextureBarrier
    {
        FTextureStateExtension* Texture = nullptr;
        uint32 MipLevel = 0;
        uint32 ArraySlice = 0;
        bool bEntireTexture = false;
        EResourceStates StateBefore = EResourceStates::Unknown;
        EResourceStates StateAfter = EResourceStates::Unknown;
    };

    struct FBufferBarrier
    {
        FBufferStateExtension* Buffer = nullptr;
        EResourceStates StateBefore = EResourceStates::Unknown;
        EResourceStates StateAfter = EResourceStates::Unknown;
    };

    class LUMINA_API FCommandListResourceStateTracker
    {
    public:

        FCommandListResourceStateTracker();
        
        void SetEnableUavBarriersForTexture(FTextureStateExtension* Texture, bool bEnableBarriers);
        void SetEnableUavBarriersForBuffer(FBufferStateExtension* Buffer, bool bEnableBarriers);

        void BeginTrackingTextureState(FTextureStateExtension* texture, FTextureSubresourceSet subresources, EResourceStates stateBits);
        void BeginTrackingBufferState(FBufferStateExtension* buffer, EResourceStates stateBits);

        void SetPermanentTextureState(FTextureStateExtension* texture, FTextureSubresourceSet subresources, EResourceStates stateBits);
        void SetPermanentBufferState(FBufferStateExtension* buffer, EResourceStates stateBits);

        EResourceStates GetTextureSubresourceState(FTextureStateExtension* texture, uint32 arraySlice, uint32 mipLevel);
        EResourceStates GetBufferState(FBufferStateExtension* buffer);

        // Internal interface
        
        void RequireTextureState(FTextureStateExtension* texture, FTextureSubresourceSet subresources, EResourceStates state);
        void RequireBufferState(FBufferStateExtension* buffer, EResourceStates state);

        void KeepBufferInitialStates();
        void KeepTextureInitialStates();
        void CommandListSubmitted();

        NODISCARD const TFixedVector<FTextureBarrier, 24>& GetTextureBarriers() const { return TextureBarriers; }
        NODISCARD const TFixedVector<FBufferBarrier, 24>& GetBufferBarriers() const { return BufferBarriers; }
        
        void ClearBarriers() { TextureBarriers.clear(); BufferBarriers.clear(); }

    private:
        
        TFixedHashMap<FTextureStateExtension*, FTextureState*, 24> TextureStates;
        TFixedHashMap<FBufferStateExtension*, FBufferState*, 24> BufferStates;

        // Deferred transitions of textures and buffers to permanent states.
        // They are executed only when the command list is executed, not when the app calls setPermanentTextureState or setPermanentBufferState.
        TFixedVector<TPair<FTextureStateExtension*, EResourceStates>, 24> PermanentTextureStates;
        TFixedVector<TPair<FBufferStateExtension*, EResourceStates>, 24> PermanentBufferStates;

        TFixedVector<FTextureBarrier, 24> TextureBarriers;
        TFixedVector<FBufferBarrier, 24> BufferBarriers;

        FTextureState* GetTextureStateTracking(FTextureStateExtension* Texture, bool bAllowCreate);
        FBufferState* GetBufferStateTracking(FBufferStateExtension* Buffer, bool bAllowCreate);


        FBlockLinearAllocator LinearAllocator;
    };
    
}
