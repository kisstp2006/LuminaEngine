#include "BindingCache.h"

#include "RenderContext.h"
#include "RHIGlobals.h"
#include "Core/Math/Hash/Hash.h"

namespace Lumina
{
    FRHIBindingLayout* FBindingCache::GetOrCreateBindingLayout(const FBindingLayoutDesc& Desc)
    {
        SIZE_T Hash = Hash::GetHash(Desc);

        FScopeLock Lock(Mutex);
        auto It = BindingLayouts.find(Hash);
        if (It != BindingLayouts.end())
        {
            return It->second;
        }

        FRHIBindingLayoutRef Layout = GRenderContext->CreateBindingLayout(Desc);
        BindingLayouts.insert_or_assign(Hash, Layout);
        
        return Layout;
    }

    FRHIBindingSet* FBindingCache::GetOrCreateBindingSet(const FBindingSetDesc& Desc, FRHIBindingLayout* Layout)
    {
        SIZE_T Hash = Hash::GetHash(Desc);
        Hash::HashCombine(Hash, Layout);

        FScopeLock Lock(Mutex);
        auto It = BindingSets.find(Hash);
        if (It != BindingSets.end())
        {
            return It->second;
        }

        FRHIBindingSetRef Set = GRenderContext->CreateBindingSet(Desc, Layout);
        BindingSets.insert_or_assign(Hash, Set);
        
        return Set;
    }

    void FBindingCache::ReleaseResources()
    {
        FScopeLock Lock(Mutex);
        BindingLayouts.clear();
        BindingSets.clear();
    }
}
