#include "RenderGraph.h"

#include "RenderGraphContext.h"
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
    constexpr size_t InitialLinearAllocatorSize = 1024 * 10;
    
    FRenderGraph::FRenderGraph()
        : GraphAllocator(InitialLinearAllocatorSize)
    {
        Passes.reserve(12);
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
        THashMap<Threading::ThreadID, FRHICommandListRef> UsedCommandLists;
        TVector<ICommandList*> Whatever;
        
        FMutex Garbage;
        for (const FRGPassGroup& Group : ParallelGroups)
        {
            if (Group.Passes.size() == 1)
            {
                FRHICommandListRef CommandList;

                if (UsedCommandLists.find(Threading::GetThreadID()) != UsedCommandLists.end())
                {
                    CommandList = UsedCommandLists[Threading::GetThreadID()];
                }
                else
                {
                    CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                    CommandList->Open();

                    UsedCommandLists.emplace(Threading::GetThreadID(), CommandList);
                    Whatever.push_back(CommandList);
                }

                FRGPassHandle Pass = Group.Passes[0];
                Pass->Execute(*CommandList);
            }
            else
            {
                Task::ParallelFor(Group.Passes.size(), [&](uint32 Index)
                {
                    FRHICommandListRef CommandList;

                    {
                        FScopeLock Lock(Garbage);

                        if (UsedCommandLists.find(Threading::GetThreadID()) != UsedCommandLists.end())
                        {
                            CommandList = UsedCommandLists[Threading::GetThreadID()];
                        }
                        else
                        {
                            CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                            CommandList->Open();

                            UsedCommandLists.emplace(Threading::GetThreadID(), CommandList);
                            Whatever.push_back(CommandList);
                        }
                    }
                    FRGPassHandle Pass = Group.Passes[Index];
                    Pass->Execute(*CommandList);
                }, ETaskPriority::High);
            }
        }

        for (auto& [ID, CommandList] : UsedCommandLists)
        {
            CommandList->Close();
        }
        
        GRenderContext->ExecuteCommandLists(Whatever.data(), Whatever.size(), ECommandQueue::Graphics);
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
