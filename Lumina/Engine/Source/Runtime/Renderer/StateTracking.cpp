#include "StateTracking.h"

#include <ranges>

#include "RenderResource.h"
#include "Core/Profiler/Profile.h"

namespace Lumina
{
    namespace VkStateTracking
    {
        bool VerifyPermanentResourceState(EResourceStates PermanentState, EResourceStates RequiredState, bool bIsTexture, FStringView DebugName)
        {
            if ((PermanentState & RequiredState) != RequiredState)
            {
                std::ostringstream ossRequired, ossPresent;
                ossRequired << "0x" << std::hex << std::uppercase << uint32(RequiredState);
                ossPresent  << "0x" << std::hex << std::uppercase << uint32(PermanentState);

                LOG_ERROR("Permanent {0} - {1} doesn't have the right state bits. Requires: {2}, Present: {3}",
                    bIsTexture ? "Texture" : "Buffer", DebugName, ossRequired.str(), ossPresent.str());

                return false;
            }

            return true;
        }
    }

    
    namespace
    {
    
        uint32 CalcSubresource(uint32 MipLevel, uint32 ArraySlice, const FRHIImageDesc& Desc)
        {
            return MipLevel + ArraySlice * Desc.NumMips;
        }

    }

    FCommandListResourceStateTracker::FCommandListResourceStateTracker()
        : LinearAllocator(1024)
    {
        
    }

    void FCommandListResourceStateTracker::SetEnableUavBarriersForTexture(FTextureStateExtension* Texture, bool bEnableBarriers)
    {
        FTextureState* tracking = GetTextureStateTracking(Texture, true);

        tracking->bEnableUavBarriers = bEnableBarriers;
        tracking->bFirstUavBarrierPlaced = false;
    }

    void FCommandListResourceStateTracker::SetEnableUavBarriersForBuffer(FBufferStateExtension* Buffer, bool bEnableBarriers)
    {
        FBufferState* tracking = GetBufferStateTracking(Buffer, true);

        tracking->enableUavBarriers = bEnableBarriers;
        tracking->firstUavBarrierPlaced = false;
    }

    void FCommandListResourceStateTracker::BeginTrackingTextureState(FTextureStateExtension* texture, FTextureSubresourceSet subresources, EResourceStates stateBits)
    {
        LUMINA_PROFILE_SCOPE();

        const FRHIImageDesc& desc = texture->DescRef;

        FTextureState* tracking = GetTextureStateTracking(texture, true);
        
        subresources = subresources.Resolve(desc, false);

        if (subresources.IsEntireTexture(desc))
        {
            tracking->State = stateBits;
            tracking->SubresourceStates.clear();
        }
        else
        {
            tracking->SubresourceStates.resize(desc.NumMips * desc.ArraySize, tracking->State);
            tracking->State = EResourceStates::Unknown;

            for (uint32 mipLevel = subresources.BaseMipLevel; mipLevel < subresources.BaseMipLevel + subresources.NumMipLevels; mipLevel++)
            {
                for (uint32 arraySlice = subresources.BaseArraySlice; arraySlice < subresources.BaseArraySlice + subresources.NumArraySlices; arraySlice++)
                {
                    uint32 subresource = CalcSubresource(mipLevel, arraySlice, desc);
                    tracking->SubresourceStates[subresource] = stateBits;
                }
            }
        }
    }

    void FCommandListResourceStateTracker::BeginTrackingBufferState(FBufferStateExtension* buffer, EResourceStates stateBits)
    {
        FBufferState* tracking = GetBufferStateTracking(buffer, true);

        tracking->state = stateBits;
    }

    void FCommandListResourceStateTracker::SetPermanentTextureState(FTextureStateExtension* texture, FTextureSubresourceSet subresources, EResourceStates stateBits)
    {
        const FRHIImageDesc& desc = texture->DescRef;

        subresources = subresources.Resolve(desc, false);

        bool permanent = true;
        if (!subresources.IsEntireTexture(desc))
        {
            LOG_ERROR("Attempted to perform a permanent state transition on a subset of subresources of texture");
            permanent = false;
        }

        RequireTextureState(texture, subresources, stateBits);

        if (permanent)
        {
            PermanentTextureStates.push_back(eastl::make_pair(texture, stateBits));
            GetTextureStateTracking(texture, true)->bPermanentTransition = true;
        }
    }

    void FCommandListResourceStateTracker::SetPermanentBufferState(FBufferStateExtension* buffer, EResourceStates stateBits)
    {
        RequireBufferState(buffer, stateBits);

        PermanentBufferStates.push_back(eastl::make_pair(buffer, stateBits));
    }

    EResourceStates FCommandListResourceStateTracker::GetTextureSubresourceState(FTextureStateExtension* texture, uint32 arraySlice, uint32 mipLevel)
    {
        FTextureState* tracking = GetTextureStateTracking(texture, false);
        if (!tracking)
        {
            return texture->DescRef.bKeepInitialState ? 
                (texture->stateInitialized ? texture->DescRef.InitialState : EResourceStates::Common) :
                EResourceStates::Unknown;
        }

        // whole resource
        if (tracking->SubresourceStates.empty())
        {
            return tracking->State;
        }

        uint32 subresource = CalcSubresource(mipLevel, arraySlice, texture->DescRef);
        return tracking->SubresourceStates[subresource];
    }

    EResourceStates FCommandListResourceStateTracker::GetBufferState(FBufferStateExtension* buffer)
    {
        FBufferState* tracking = GetBufferStateTracking(buffer, false);

        if (!tracking)
        {
            return EResourceStates::Unknown;
        }

        return tracking->state;
    }

    void FCommandListResourceStateTracker::RequireTextureState(FTextureStateExtension* texture, FTextureSubresourceSet subresources, EResourceStates state)
    {
        LUMINA_PROFILE_SCOPE();

        if (texture->PermanentState != EResourceStates::Unknown)
        {
            VkStateTracking::VerifyPermanentResourceState(texture->PermanentState, state, true, texture->DescRef.DebugName);
            return;
        }

        subresources = subresources.Resolve(texture->DescRef, false);

        FTextureState* tracking = GetTextureStateTracking(texture, true);
        
        if (subresources.IsEntireTexture(texture->DescRef) && tracking->SubresourceStates.empty())
        {
            // We're requiring state for the entire texture, and it's been tracked as entire texture too

            bool transitionNecessary = tracking->State != state;
            bool uavNecessary = ((state & EResourceStates::UnorderedAccess) != EResourceStates::Unknown)
                && (tracking->bEnableUavBarriers || !tracking->bFirstUavBarrierPlaced);

            if (transitionNecessary || uavNecessary)
            {
                TextureBarriers.emplace_back(texture, 0, 0, true, tracking->State, state);
            }

            tracking->State = state;

            if (uavNecessary && !transitionNecessary)
            {
                tracking->bFirstUavBarrierPlaced = true;
            }
        }
        else
        {
            // Transition individual subresources

            // Make sure that we're tracking the texture on subresource level
            bool stateExpanded = false;
            if (tracking->SubresourceStates.empty())
            {
                if (tracking->State == EResourceStates::Unknown)
                {
                    LOG_ERROR("Unknown prior state of texture {0}. Call CommandList::BeginTrackingTextureState(...) before using the texture or use the KeepInitialState and InitialState members of FRHIImageDesc.",
                        texture->DescRef.DebugName);
                }

                tracking->SubresourceStates.resize(texture->DescRef.NumMips * texture->DescRef.ArraySize, tracking->State);
                tracking->State = EResourceStates::Unknown;
                stateExpanded = true;
            }
            
            bool anyUavBarrier = false;

            for (uint32 arraySlice = subresources.BaseArraySlice; arraySlice < subresources.BaseArraySlice + subresources.NumArraySlices; arraySlice++)
            {
                for (uint32 mipLevel = subresources.BaseMipLevel; mipLevel < subresources.BaseMipLevel + subresources.NumMipLevels; mipLevel++)
                {
                    uint32 subresourceIndex = CalcSubresource(mipLevel, arraySlice, texture->DescRef);

                    auto priorState = tracking->SubresourceStates[subresourceIndex];

                    if (priorState == EResourceStates::Unknown && !stateExpanded)
                    {
                        LOG_ERROR("Unknown prior state of texture \"{0}\" subresource (MipLevel = {1}, ArraySlice = {2}). Call CommandList::BeginTrackingTextureState(...) before using the texture or use the keepInitialState and initialState members of TextureDesc.",
                                  texture->DescRef.DebugName, mipLevel, arraySlice);
                    }
                    
                    bool transitionNecessary = priorState != state;
                    bool uavNecessary = ((state & EResourceStates::UnorderedAccess) != EResourceStates::Unknown)
                        && !anyUavBarrier && (tracking->bEnableUavBarriers || !tracking->bFirstUavBarrierPlaced);

                    if (transitionNecessary || uavNecessary)
                    {
                        TextureBarriers.emplace_back(FTextureBarrier
                        {
                            .Texture = texture,
                            .MipLevel = mipLevel,
                            .ArraySlice = arraySlice,
                            .bEntireTexture = false,
                            .StateBefore = priorState,
                            .StateAfter = state
                        });
                    }

                    tracking->SubresourceStates[subresourceIndex] = state;

                    if (uavNecessary && !transitionNecessary)
                    {
                        anyUavBarrier = true;
                        tracking->bFirstUavBarrierPlaced = true;
                    }
                }
            }
        }
    }

    void FCommandListResourceStateTracker::RequireBufferState(FBufferStateExtension* buffer, EResourceStates state)
    {
        LUMINA_PROFILE_SCOPE();

        if (buffer->DescRef.Usage.IsFlagSet(EBufferUsageFlags::Dynamic))
        {
            return;
        }

        if (buffer->PermanentState != EResourceStates::Unknown)
        {
            VkStateTracking::VerifyPermanentResourceState(buffer->PermanentState, state, false, buffer->DescRef.DebugName);
            return;
        }

        if (buffer->DescRef.Usage.IsFlagSet(EBufferUsageFlags::CPUWritable))
        {
            // CPU-visible buffers can't change state
            return;
        }

        FBufferState* tracking = GetBufferStateTracking(buffer, true);

        if (tracking->state == EResourceStates::Unknown)
        {
            LOG_ERROR("Unknown prior state of buffer \"{0}\". "
                      "Call CommandList::BeginTrackingBufferState(...) before using the buffer or use the "
                      "keepInitialState and initialState members of BufferDesc.",
                      buffer->DescRef.DebugName);
        }

        bool transitionNecessary = tracking->state != state;
        bool uavNecessary = ((state & EResourceStates::UnorderedAccess) != EResourceStates::Unknown) && (tracking->enableUavBarriers || !tracking->firstUavBarrierPlaced);

        if (transitionNecessary)
        {
            // See if this buffer is already used for a different purpose in this batch.
            // If it is, combine the state bits.
            // Example: same buffer used as index and vertex buffer, or as SRV and indirect arguments.
            for (FBufferBarrier& barrier : BufferBarriers)
            {
                if (barrier.Buffer == buffer)
                {
                    barrier.StateAfter = EResourceStates(barrier.StateAfter | state);
                    tracking->state = barrier.StateAfter;
                    return;
                }
            }
        }

        if (transitionNecessary || uavNecessary)
        {
            BufferBarriers.emplace_back(FBufferBarrier
            {
                .Buffer = buffer,
                .StateBefore = tracking->state,
                .StateAfter = state,
            });
        }

        if (uavNecessary && !transitionNecessary)
        {
            tracking->firstUavBarrierPlaced = true;
        }
    
        tracking->state = state;
    }

    void FCommandListResourceStateTracker::KeepBufferInitialStates()
    {
        LUMINA_PROFILE_SCOPE();
        
        for (auto& [buffer, tracking] : BufferStates)
        {
            if (buffer->DescRef.bKeepInitialState && !buffer->PermanentState && !buffer->DescRef.Usage.IsFlagSet(EBufferUsageFlags::Dynamic) &&!tracking->permanentTransition)
            {
                RequireBufferState(buffer, buffer->DescRef.InitialState);
            }
        }
    }

    void FCommandListResourceStateTracker::KeepTextureInitialStates()
    {
        LUMINA_PROFILE_SCOPE();

        for (auto& [texture, tracking] : TextureStates)
        {
            if (texture->DescRef.bKeepInitialState && !texture->PermanentState && !tracking->bPermanentTransition)
            {
                RequireTextureState(texture, AllSubresources, texture->DescRef.InitialState);
            }
        }
    }

    void FCommandListResourceStateTracker::CommandListSubmitted()
    {
        LUMINA_PROFILE_SCOPE();

        for (auto& [texture, state] : PermanentTextureStates)
        {
            if (texture->PermanentState != EResourceStates::Unknown && texture->PermanentState != state)
            {
                LOG_ERROR("Attempted to switch permanent state of texture {0} from 0x{1:X} to 0x{2:X}.",
                          texture->DescRef.DebugName,
                          static_cast<uint32>(texture->PermanentState),
                          static_cast<uint32>(state));
                continue;
            }

            texture->PermanentState = state;
        }
        
        PermanentTextureStates.clear();

        for (auto& [buffer, state] : PermanentBufferStates)
        {
            if (buffer->PermanentState != EResourceStates::Unknown && buffer->PermanentState != state)
            {
                LOG_ERROR("Attempted to switch permanent state of buffer {0} from 0x{1:X} to 0x{2:X}.",
                          buffer->DescRef.DebugName,
                          static_cast<uint32>(buffer->PermanentState),
                          static_cast<uint32>(state));
                continue;
            }

            buffer->PermanentState = state;
        }
        
        PermanentBufferStates.clear();

        for (auto& [texture, State] : TextureStates)
        {
            if (texture->DescRef.bKeepInitialState && !texture->stateInitialized)
            {
                texture->stateInitialized = true;
            }
        }

        TextureStates.clear(); 
        BufferStates.clear();
        LinearAllocator.Reset();
    }

    FTextureState* FCommandListResourceStateTracker::GetTextureStateTracking(FTextureStateExtension* Texture, bool bAllowCreate)
    {
        auto it = TextureStates.find(Texture);

        if (it != TextureStates.end())
        {
            return it->second;
        }

        if (!bAllowCreate)
        {
            return nullptr;
        }

        FTextureState* TrackingRef = LinearAllocator.TAlloc<FTextureState>();
        TextureStates.emplace(Texture, TrackingRef);
        
        if (Texture->DescRef.bKeepInitialState)
        {
            TrackingRef->State = Texture->stateInitialized ? Texture->DescRef.InitialState : EResourceStates::Common;
        }

        return TrackingRef;
    }

    FBufferState* FCommandListResourceStateTracker::GetBufferStateTracking(FBufferStateExtension* Buffer, bool bAllowCreate)
    {
        auto it = BufferStates.find(Buffer);

        if (it != BufferStates.end())
        {
            return it->second;
        }

        if (!bAllowCreate)
        {
            return nullptr;
        }

        FBufferState* TrackingRef = LinearAllocator.TAlloc<FBufferState>();
        BufferStates.emplace(Buffer, TrackingRef);
        
        if (Buffer->DescRef.bKeepInitialState)
        {
            TrackingRef->state = Buffer->DescRef.InitialState;
        }

        return TrackingRef;
    }
}
