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

            TFixedVector<FLambdaTask*, 4> AsyncComputeTasks;
            TFixedVector<ICommandList*, 30> AllCommandLists;
            AllCommandLists.reserve(Passes.size());
            
            uint32 TotalOffset = 0;

            for (int GroupIndex = 0; GroupIndex < ParallelGroups.size(); ++GroupIndex)
            {
                LUMINA_PROFILE_SECTION("Render Graph Parallel Group");
                const FRGPassGroup& Group = ParallelGroups[GroupIndex];

                uint32 GroupOffset = TotalOffset;
                for (int j = 0; j < Group.Passes.size(); ++j)
                {
                    AllCommandLists.push_back(nullptr);
                }
                
                Task::ParallelFor(Group.Passes.size(), 1, [GroupIndex, GroupOffset, &AsyncComputeTasks, &AllCommandLists, this](uint32 PassIndex)
                {
                    uint32 Index = GroupOffset + PassIndex;
                    if (ParallelGroups[GroupIndex].Passes[PassIndex]->GetDescriptor()->HasAnyFlag(ERGExecutionFlags::Async))
                    {
                        FLambdaTask* Task = Task::AsyncTask(1, 1, [this, &AllCommandLists, GroupIndex, PassIndex, Index](uint32 Start, uint32 End, uint32 Thread)
                        {
                            FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                            CommandList->Open();
                            FRGPassHandle Pass = ParallelGroups[GroupIndex].Passes[PassIndex];
                            Pass->Execute(*CommandList);
                            CommandList->Close();
                            AllCommandLists[Index] = CommandList.GetReference();
                        });

                        AsyncComputeTasks.push_back(Task);
                    }
                    else
                    {
                        FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                
                        CommandList->Open();
                        FRGPassHandle Pass = ParallelGroups[GroupIndex].Passes[PassIndex];
                        Pass->Execute(*CommandList);
                        CommandList->Close();
                
                        AllCommandLists[Index] = CommandList.GetReference();
                    }

                });

                
                TotalOffset += Group.Passes.size();
            }

            for (FLambdaTask* Task : AsyncComputeTasks)
            {
                GTaskSystem->WaitForTask(Task);
            }
            AsyncComputeTasks.clear();
            
            GRenderContext->ExecuteCommandLists(AllCommandLists.data(), AllCommandLists.size(), ECommandQueue::Graphics);
        }
        else
        {
            TFixedVector<FLambdaTask*, 4> AsyncTasks;
            TFixedVector<ICommandList*, 1> AllCommandLists;
        
            FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
            AllCommandLists.push_back(CommandList);
            CommandList->Open();

            for (int PassIndex = 0; PassIndex < Passes.size(); ++PassIndex)
            {
                FRGPassHandle Pass = Passes[PassIndex];
                
                // The user has promised us this pass can now run at any time without issues, so we dispatch it and keep going.
                if (Pass->GetDescriptor()->HasAnyFlag(ERGExecutionFlags::Async))
                {
                    uint32 AsyncPassIndex = AllCommandLists.size();
                    AllCommandLists.push_back(nullptr);
                    
                    FLambdaTask* Task = Task::AsyncTask(1, 1, [Pass, AsyncPassIndex, &AllCommandLists](uint32 Start, uint32 End, uint32 Thread)
                    {
                        FRHICommandListRef LocalCommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                        
                        AllCommandLists[AsyncPassIndex] = LocalCommandList;

                        LocalCommandList->Open();
                        Pass->Execute(*LocalCommandList);
                        LocalCommandList->Close();
                    });

                    AsyncTasks.push_back(Task);
                }
                else // Run the pass serially.
                {
                    Pass->Execute(*CommandList);
                }
            }

            CommandList->Close();

            for (FLambdaTask* Task : AsyncTasks)
            {
                GTaskSystem->WaitForTask(Task);
            }
            
            GRenderContext->ExecuteCommandLists(AllCommandLists.data(), AllCommandLists.size(), ECommandQueue::Graphics);   
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
