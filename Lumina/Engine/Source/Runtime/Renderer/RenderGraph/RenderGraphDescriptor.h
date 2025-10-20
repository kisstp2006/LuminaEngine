#pragma once

#include "Renderer/RenderResource.h"
#include "Renderer/RHIFwd.h"


namespace Lumina
{
    class FRGImage;
    class FRGBuffer;
    class FRGResource;
}


namespace Lumina
{

    class FRGPassDescriptor
    {
        friend class FRGPassAnalyzer;
        
    public:

        void AddBinding(FRHIBindingSet* Binding);
        void AddRawWrite(const IRHIResource* InResource);
        void AddRawRead(const IRHIResource* InResource);

    private:
        
        TFixedVector<FRHIBindingSet*, 2> Bindings;
        TFixedVector<const IRHIResource*, 4> RawWrites;
        TFixedVector<const IRHIResource*, 4> RawReads;

    };
}
