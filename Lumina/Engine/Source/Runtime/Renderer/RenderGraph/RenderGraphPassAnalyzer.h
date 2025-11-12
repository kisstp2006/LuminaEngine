#pragma once
#include "Containers/Array.h"
#include "RenderGraphTypes.h"

namespace Lumina
{
    class IRHIResource;

    class FRGPassAnalyzer
    {
    public:

        TVector<FRGPassGroup> AnalyzeParallelPasses(const TVector<FRGPassHandle>& Passes);
        uint32 HighestParallelGroupCount = 0;

    private:

        struct alignas(64) FPassResourceAccess
        {
            FRGPassHandle Pass;
            THashSet<const IRHIResource*> Reads;
            THashSet<const IRHIResource*> Writes;
        };

        
        TVector<FRGPassGroup> GroupPasses(const TVector<FRGPassHandle>& Passes, const TVector<TVector<int>>& Dependencies);
        FPassResourceAccess AnalyzePassResources(FRGPassHandle Pass);
        TVector<TVector<int>> BuildDependencyGraph(const TVector<FPassResourceAccess>& PassAccess);
        bool HasDependency(const FPassResourceAccess& PreviousPass, const FPassResourceAccess& CurrentPass);
        void AnalyzeLastResourceUsages(TSpan<FPassResourceAccess> PassResources);

        THashMap<const IRHIResource*, FRGPassHandle> FirstWriter;
        THashMap<const IRHIResource*, FRGPassHandle> LastReader;
        
    };
}
