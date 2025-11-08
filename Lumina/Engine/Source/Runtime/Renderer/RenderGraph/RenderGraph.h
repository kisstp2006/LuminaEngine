#pragma once
#include "RenderGraphContext.h"
#include "RenderGraphEvent.h"
#include "RenderGraphResourceRegistry.h"
#include "RenderGraphResources.h"
#include "RenderGraphTypes.h"
#include "Containers/Array.h"
#include "Memory/Allocators/Allocator.h"


namespace Lumina
{
    class FRGPassDescriptor;
    struct FBindingLayoutDesc;
    class FRHIBindingSet;
    class FRenderGraphPass;
}



namespace Lumina
{
    template <typename ExecutorType>
    concept ExecutorConcept = (sizeof(ExecutorType) <= 1024) && eastl::is_invocable_v<ExecutorType, ICommandList&>;
    
    class LUMINA_API FRenderGraph
    {
    public:

        FRenderGraph();
        
        template <ERGPassFlags PassFlags, ExecutorConcept ExecutorType>
        FRGPassHandle AddPass(FRGEvent&& Event, const FRGPassDescriptor* Parameters, ExecutorType&& Executor);

        FRGPassDescriptor* AllocDescriptor();
        
        void Execute();
        void Compile();

        void AllocateTransientResources();
        
        template<typename TResource, typename TDescription>
        TResource* AllocResource(TDescription&& Desc)
        {
            TResource* NewResource = nullptr;
            if constexpr (std::is_same_v<TResource, FRGBuffer>)
            {
                NewResource = CreateBuffer(std::forward<TDescription>(Desc));
            }
            else if constexpr (std::is_same_v<TResource, FRGImage>)
            {
                NewResource = CreateImage(std::forward<TDescription>(Desc));
            }

            return NewResource;
        }

    private:
        
        template<typename ExecutorType>
        FRGPassHandle AddPassInternal(FRGEvent&& Event, const FRGPassDescriptor* Parameters, ERGPassFlags Flags, ExecutorType&& Executor);


        FRGBuffer* CreateBuffer(const FRHIBufferDesc& Desc);
        FRGImage* CreateImage(const FRHIImageDesc& Desc);

    private:

        struct FResourceOp
        {
            enum
            {
                Alloc,
                Dealloc,
            };
        };
        

    private:

        TRGHandleRegistry<FRGBuffer>    BufferRegistry;
        TRGHandleRegistry<FRGImage>     ImageRegistry;
        
        FBlockLinearAllocator           GraphAllocator;
        TVector<FRGPassHandle>          Passes;
        TVector<FRGPassGroup>           ParallelGroups;

        uint32                          HighestGroupCount = 0;
        uint32                          TotalPasses = 0;
    };
}

#include "RenderGraph.inl"