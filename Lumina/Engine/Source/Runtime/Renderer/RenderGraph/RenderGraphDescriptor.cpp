#include "RenderGraphDescriptor.h"

namespace Lumina
{
    void FRGPassDescriptor::AddBinding(FRHIBindingSet* Binding)
    {
        Bindings.push_back(Binding);
    }

    void FRGPassDescriptor::AddRawWrite(const IRHIResource* InResource)
    {
        RawWrites.push_back(InResource);
    }

    void FRGPassDescriptor::AddRawRead(const IRHIResource* InResource)
    {
        RawReads.push_back(InResource);
    }
}
