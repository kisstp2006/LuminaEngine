#include "RenderGraph.h"
#include "RenderGraphDescriptor.h"
#include "RenderGraphPass.h"
#include "RenderGraphPassAnalyzer.h"
#include "RenderGraphResources.h"
#include "Core/Engine/Engine.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIGlobals.h"
#include "TaskSystem/TaskSystem.h"


namespace Lumina
{
    FRenderGraph::FRenderGraph()
        : GraphAllocator(1024)
    {
        Passes.reserve(12);
        HighestGroupCount = 0;
    }

    FRGPassDescriptor* FRenderGraph::AllocDescriptor()
    {
        return GraphAllocator.TAlloc<FRGPassDescriptor>();
    }

    void FRenderGraph::Execute()
    {
        LUMINA_PROFILE_SCOPE();

        Compile();
        AllocateTransientResources();

#if 1
        
        TVector<TVector<ICommandList*>> CommandListGroups(ParallelGroups.size());
        
        Task::ParallelFor(ParallelGroups.size(), ParallelGroups.size() / 2, [&](uint32 Index)
        {
            TVector<ICommandList*>& GroupCommandLists = CommandListGroups[Index];
            GroupCommandLists.resize(ParallelGroups[Index].Passes.size());
            
            for (int i = 0; i < ParallelGroups[Index].Passes.size(); ++i)
            {
                FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                CommandList->Open();
                GroupCommandLists[i] = CommandList;
            }
        });

        for (int i = 0; i < ParallelGroups.size(); ++i)
        {
            const FRGPassGroup& Group = ParallelGroups[i];
            
            if (Group.Passes.size() == 1)
            {
                ICommandList* CommandList = CommandListGroups[i][0];
                FRGPassHandle Pass = Group.Passes[0];
                Pass->Execute(*CommandList);
            }
            else
            {
                Task::ParallelFor(Group.Passes.size(), Group.Passes.size() / 4, [&](uint32 Index)
                {
                    ICommandList* CommandList = CommandListGroups[i][Index];
                    FRGPassHandle Pass = Group.Passes[Index];
                    Pass->Execute(*CommandList);
                    
                }, ETaskPriority::High);
            }            
        }

        Task::ParallelFor(ParallelGroups.size(), ParallelGroups.size() / 2, [&](uint32 Index)
        {
            TVector<ICommandList*>& GroupCommandLists = CommandListGroups[Index];
            for (ICommandList* GroupCommandList : GroupCommandLists)
            {
                GroupCommandList->Close();
            }
        }, ETaskPriority::High);

        TVector<ICommandList*> FlatCommandLists;
        FlatCommandLists.reserve(CommandListGroups.size() * 2);
        
        for (auto& GroupCommandList : CommandListGroups)
        {
            for (ICommandList* CommandList : GroupCommandList)
            {
                FlatCommandLists.push_back(CommandList);
            }
        }
        
        GRenderContext->ExecuteCommandLists(FlatCommandLists.data(), (uint32)FlatCommandLists.size(), ECommandQueue::Graphics);

        
#else

        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
        CommandList->Open();
        
        for (const FRGPassGroup& Group : ParallelGroups)
        {
            
            for (FRGPassHandle Pass : Group.Passes)
            {
                Pass->Execute(*CommandList);
            }

        }
        
        CommandList->Close();

        GRenderContext->ExecuteCommandList(CommandList);

#endif
    }

    void FRenderGraph::Compile()
    {
        LUMINA_PROFILE_SCOPE();
        
        FRGPassAnalyzer Analyzer;
        ParallelGroups = Analyzer.AnalyzeParallelPasses(Passes);
        HighestGroupCount = Analyzer.HighestParallelGroupCount;
    }

    void FRenderGraph::AllocateTransientResources()
    {
        for (const FRGBuffer* Buffer : BufferRegistry)
        {
            
        }

        for (const FRGImage* Image : ImageRegistry)
        {

        }
    }

    FRGBuffer* FRenderGraph::CreateBuffer(const FRHIBufferDesc& Desc)
    {
        return BufferRegistry.Allocate(GraphAllocator);
    }

    FRGImage* FRenderGraph::CreateImage(const FRHIImageDesc& Desc)
    {
        return ImageRegistry.Allocate(GraphAllocator);
    }
}
