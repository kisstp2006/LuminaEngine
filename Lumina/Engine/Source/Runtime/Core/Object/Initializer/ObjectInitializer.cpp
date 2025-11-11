#include "ObjectInitializer.h"

#include "Containers/Array.h"
#include "Core/Threading/Thread.h"

namespace Lumina
{
    static thread_local TFixedVector<FObjectInitializer*, 4> InitializerStack;

    FObjectInitializer::FObjectInitializer(CObject* Obj, CPackage* InPackage, const FConstructCObjectParams& InParams)
        : Object(Obj)
        , Package(InPackage)
        , Params(InParams)
    {
        Construct();
    }

    FObjectInitializer::~FObjectInitializer()
    {
        InitializerStack.pop_back();
    }

    FObjectInitializer* FObjectInitializer::Get()
    {
        return InitializerStack.empty() ? nullptr : InitializerStack.back();
    }

    void FObjectInitializer::Construct()
    {
        InitializerStack.push_back(this);
    }
}
