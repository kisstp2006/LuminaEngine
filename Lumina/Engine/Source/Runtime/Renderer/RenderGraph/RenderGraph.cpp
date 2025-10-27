#include "RenderGraph.h"
#include "RenderGraphDescriptor.h"
#include "RenderGraphPass.h"
#include "RenderGraphPassAnalyzer.h"
#include "RenderGraphResources.h"
#include "Core/Engine/Engine.h"
#include "Platform/Process/PlatformProcess.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIGlobals.h"
#include "TaskSystem/TaskSystem.h"


namespace Lumina
{
    FRenderGraph::FRenderGraph()
        : GraphAllocator(1024)
    {
        Passes.reserve(24);
        HighestGroupCount = 0;
    }

    FRGPassDescriptor* FRenderGraph::AllocDescriptor()
    {
        return GraphAllocator.TAlloc<FRGPassDescriptor>();
    }

    void FRenderGraph::Execute()
    {
        LUMINA_PROFILE_SCOPE();
        AllocateTransientResources();

        if (Passes.size() > 1000) // Pretty much disabled for now because it's so much slower.
        {

            Compile();
            
            struct CommandListBatch
            {
                ICommandList* CommandList = nullptr;
                uint32 GroupIndex;
                uint32 PassIndex;
            };
        
            TVector<CommandListBatch> CommandListBatches;
        
            for (int i = 0; i < ParallelGroups.size(); ++i)
            {
                const FRGPassGroup& Group = ParallelGroups[i];
            
                if (Group.Passes.size() == 1)
                {
                    CommandListBatch Batch;
                    Batch.GroupIndex = i;
                    Batch.PassIndex = 0;
                    CommandListBatches.push_back(Batch);
                }
                else
                {
                    for (uint32 PassIndex = 0; PassIndex < Group.Passes.size(); ++PassIndex)
                    {
                        CommandListBatch Batch;
                        Batch.GroupIndex = i;
                        Batch.PassIndex = PassIndex;
                        CommandListBatches.push_back(Batch);
                    }
                }
            }
        
            uint64 WorkGrain = Math::Max<uint64>(1u, CommandListBatches.size() / 4);
            Task::ParallelFor(CommandListBatches.size(), WorkGrain, [&](uint32 Index)
            {
                FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                CommandList->Open();
                CommandListBatches[Index].CommandList = CommandList;
            });
        
            uint32 BatchIndex = 0;
            for (int i = 0; i < ParallelGroups.size(); ++i)
            {
                const FRGPassGroup& Group = ParallelGroups[i];
            
                if (Group.Passes.size() == 1)
                {
                    ICommandList* CommandList = CommandListBatches[BatchIndex].CommandList;
                    Group.Passes[0]->Execute(*CommandList);
                    BatchIndex++;
                }
                else
                {
                    uint32 GroupBatchStart = BatchIndex;
                    Task::ParallelFor(Group.Passes.size(), Group.Passes.size() / 4, [&](uint32 PassIndex)
                    {
                        ICommandList* CommandList = CommandListBatches[GroupBatchStart + PassIndex].CommandList;
                        FRGPassHandle Pass = Group.Passes[PassIndex];
                    
                        Pass->Execute(*CommandList);
                    }, ETaskPriority::High);
                
                    BatchIndex += Group.Passes.size();
                }
            }
        
            Task::ParallelFor(CommandListBatches.size(), WorkGrain, [&](uint32 Index)
            {
                CommandListBatches[Index].CommandList->Close();
            });
        
            TVector<ICommandList*> FlatCommandLists;
            FlatCommandLists.reserve(CommandListBatches.size());
            for (const CommandListBatch& Batch : CommandListBatches)
            {
                FlatCommandLists.push_back(Batch.CommandList);
            }
        
            GRenderContext->ExecuteCommandLists(FlatCommandLists.data(), (uint32)FlatCommandLists.size(), ECommandQueue::Graphics);
        }
        else
        {
            FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
            CommandList->Open();

            for (FRGPassHandle Pass : Passes)
            {
                Pass->Execute(*CommandList);
            }
            
            CommandList->Close();
            GRenderContext->ExecuteCommandList(CommandList);   
        }
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
