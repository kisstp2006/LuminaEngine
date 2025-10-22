#include "RenderGraphPassAnalyzer.h"
#include "TaskSystem/TaskSystem.h"
#include "RenderGraphDescriptor.h"
#include "RenderGraphPass.h"

namespace Lumina
{
    TVector<FRGPassGroup> FRGPassAnalyzer::AnalyzeParallelPasses(const TVector<FRGPassHandle>& Passes)
    {
        LUMINA_PROFILE_SCOPE();

        TVector<FPassResourceAccess> PassAccess(Passes.size());

        Task::ParallelFor(Passes.size(), Passes.size() / 2, [&](uint32 Index)
        {
            FRGPassHandle Pass = Passes[Index];
            PassAccess[Index] = AnalyzePassResources(Pass);
        });

        AnalyzeLastResourceUsages(PassAccess);



        TVector<TVector<int>> Dependencies = BuildDependencyGraph(PassAccess);

        return GroupPasses(Passes, Dependencies);
    }

    TVector<FRGPassGroup> FRGPassAnalyzer::GroupPasses(const TVector<FRGPassHandle>& Passes, const TVector<TVector<int>>& Dependencies)
    {
        LUMINA_PROFILE_SCOPE();

        const int NumPasses = (int)Passes.size();

        TVector PassLayers(NumPasses, -1);
        TVector Processed(NumPasses, false);

        for (int i = 0; i < NumPasses; ++i)
        {
            if (Processed[i])
            {
                continue;
            }

            int MaxDependencyLayer = -1;
            for (int Dependency : Dependencies[i])
            {
                MaxDependencyLayer = std::max(MaxDependencyLayer, PassLayers[Dependency]);
            }

            PassLayers[i] = MaxDependencyLayer + 1;
            Processed[i] = true;
        }

        int NumLayers = 0;
        for (int Layer : PassLayers)
        {
            NumLayers = std::max(NumLayers, Layer + 1);
        }

        TVector<FRGPassGroup> ParallelGroups(NumLayers);
        for (int i = 0; i < NumPasses; ++i)
        {
            ParallelGroups[PassLayers[i]].Passes.push_back(Passes[i]);
            HighestParallelGroupCount = std::max<uint32>(HighestParallelGroupCount, ParallelGroups[PassLayers[i]].Passes.size());
        }

        return ParallelGroups;
    }

    FRGPassAnalyzer::FPassResourceAccess FRGPassAnalyzer::AnalyzePassResources(FRGPassHandle Pass)
    {
        FPassResourceAccess Access;
        Access.Pass = Pass;

        if (Pass->GetDescriptor() == nullptr)
        {
            return Access;
        }

        const FRGPassDescriptor* Descriptor = Pass->GetDescriptor();

        auto IsWrite = [] (ERHIBindingResourceType Type) -> bool
        {
            switch (Type)
            {
                case ERHIBindingResourceType::Texture_SRV:              return false;
                case ERHIBindingResourceType::Buffer_SRV:               return false;
                case ERHIBindingResourceType::Sampler:                  return false;
                case ERHIBindingResourceType::Texture_UAV:              return true;
                case ERHIBindingResourceType::Buffer_UAV:               return true;
                case ERHIBindingResourceType::Buffer_CBV:               return false;
                case ERHIBindingResourceType::Buffer_Uniform_Dynamic:   return false;
                case ERHIBindingResourceType::Buffer_Storage_Dynamic:   return false;
                case ERHIBindingResourceType::PushConstants:            return false;
            }
            LUMINA_NO_ENTRY()
            
        };

        for (FRHIBindingSet* BindingSet : Descriptor->Bindings)
        {
            for (const FBindingSetItem& Item : BindingSet->GetDesc()->Bindings)
            {
                // May be null for push constants.
                if (Item.ResourceHandle == nullptr)
                {
                    continue;
                }

                if (IsWrite(Item.Type))
                {
                    Access.Writes.insert(Item.ResourceHandle);
                }
                else
                {
                    Access.Reads.insert(Item.ResourceHandle);
                }
            }
        }

        for (const IRHIResource* Resource : Descriptor->RawWrites)
        {
            Access.Writes.insert(Resource);
        }

        for (const IRHIResource* Resource : Descriptor->RawReads)
        {
            Access.Reads.insert(Resource);
        }

        return Access;
    }

    TVector<TVector<int>> FRGPassAnalyzer::BuildDependencyGraph(const TVector<FPassResourceAccess>& PassAccess)
    {
        LUMINA_PROFILE_SCOPE();

        const int NumPasses = (int)PassAccess.size();
        TVector<TVector<int>> Dependencies(NumPasses);

        Task::ParallelFor(NumPasses, NumPasses / 4, [&](uint32 Index)
        {
            const FPassResourceAccess& CurrentPass = PassAccess[Index];

            for (int j = 0; j < Index; ++j)
            {
                const FPassResourceAccess& PreviousPass = PassAccess[j];

                if (HasDependency(PreviousPass, CurrentPass))
                {
                    Dependencies[Index].push_back(j);
                }
            }
        });

        return Dependencies;
    }

    bool FRGPassAnalyzer::HasDependency(const FPassResourceAccess& PreviousPass, const FPassResourceAccess& CurrentPass)
    {
        if (PreviousPass.Writes.empty())
        {
            return false;
        }

        if (CurrentPass.Reads.empty() && CurrentPass.Writes.empty())
        {
            return false;
        }


        if (CurrentPass.Writes.size() < CurrentPass.Reads.size())
        {
            // WAW first (Write-After-Write)
            for (const IRHIResource* Resource : CurrentPass.Writes)
            {
                if (PreviousPass.Writes.count(Resource) > 0)
                {
                    return true;
                }
            }

            // WAR (Write-After-Read)
            for (const IRHIResource* Resource : CurrentPass.Writes)
            {
                if (PreviousPass.Reads.count(Resource) > 0)
                {
                    return true;
                }
            }

            // RAW (Read-After-Write)
            for (const IRHIResource* Resource : CurrentPass.Reads)
            {
                if (PreviousPass.Writes.count(Resource) > 0)
                {
                    return true;
                }
            }
        }
        else
        {
            // RAW first (Read-After-Write)
            for (const IRHIResource* Resource : CurrentPass.Reads)
            {
                if (PreviousPass.Writes.count(Resource) > 0)
                {
                    return true;
                }
            }

            // WAR (Write-After-Read)
            for (const IRHIResource* Resource : CurrentPass.Writes)
            {
                if (PreviousPass.Reads.count(Resource) > 0)
                {
                    return true;
                }
            }

            // WAW (Write-After-Write)
            for (const IRHIResource* Resource : CurrentPass.Writes)
            {
                if (PreviousPass.Writes.count(Resource) > 0)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void FRGPassAnalyzer::AnalyzeLastResourceUsages(TSpan<FPassResourceAccess> PassResources)
    {

        for (const FPassResourceAccess& Access : PassResources)
        {
			for (const IRHIResource* ReadResource : Access.Reads)
            {
                LastReader[ReadResource] = Access.Pass;
            }

            for (const IRHIResource* WriteRessource : Access.Writes)
            {
                if (FirstWriter.find(WriteRessource) == FirstWriter.end())
                {
                    FirstWriter[WriteRessource] = Access.Pass;
                }
            }
        }
    }
}
