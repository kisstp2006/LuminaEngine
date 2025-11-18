#pragma once
#include "RHIFwd.h"
#include "Containers/Array.h"
#include "Core/Threading/Thread.h"

namespace Lumina
{
    struct FBindingSetDesc;
}

namespace Lumina
{
    struct FBindingLayoutDesc;
}

namespace Lumina
{
    class FBindingCache
    {
    public:

        FRHIBindingLayout* GetOrCreateBindingLayout(const FBindingLayoutDesc& Desc);
        FRHIBindingSet* GetOrCreateBindingSet(const FBindingSetDesc& Desc, FRHIBindingLayout* Layout);

        
        void ReleaseResources();

    private:

        FMutex Mutex;
        THashMap<SIZE_T, FRHIBindingLayoutRef> BindingLayouts;
        THashMap<SIZE_T, FRHIBindingSetRef> BindingSets;

    };
}
