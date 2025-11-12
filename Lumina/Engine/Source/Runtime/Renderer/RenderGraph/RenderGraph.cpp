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

        if (Passes.size() > 30)
        {
            Compile();

            TFixedVector<ICommandList*, 30> AllCommandLists;
            AllCommandLists.reserve(Passes.size());
            
            uint32 TotalOffset = 0;

            for (int i = 0; i < ParallelGroups.size(); ++i)
            {
                LUMINA_PROFILE_SECTION("Render Graph Parallel Group");
                const FRGPassGroup& Group = ParallelGroups[i];

                uint32 GroupOffset = TotalOffset;
                for (int j = 0; j < Group.Passes.size(); ++j)
                {
                    AllCommandLists.push_back(nullptr);
                }

                if (Group.Passes.size() <= 2)
                {
                    for (int PassIndex = 0; PassIndex < Group.Passes.size(); ++PassIndex)
                    {
                        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                    
                        CommandList->Open();
                        FRGPassHandle Pass = Group.Passes[PassIndex];
                        Pass->Execute(*CommandList);
                        CommandList->Close();
                    
                        AllCommandLists[GroupOffset + PassIndex] = CommandList.GetReference();   
                    }
                }
                else
                {
                    Task::ParallelFor(Group.Passes.size(), 1, [&](uint32 PassIndex)
                    {
                        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                    
                        CommandList->Open();
                        FRGPassHandle Pass = Group.Passes[PassIndex];
                        Pass->Execute(*CommandList);
                        CommandList->Close();
                    
                        AllCommandLists[GroupOffset + PassIndex] = CommandList.GetReference();

                    });
                }
                TotalOffset += Group.Passes.size();
                
            }

            GRenderContext->ExecuteCommandLists(AllCommandLists.data(), AllCommandLists.size(), ECommandQueue::Graphics);

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
