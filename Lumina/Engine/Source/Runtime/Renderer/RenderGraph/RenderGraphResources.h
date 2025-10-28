#pragma once
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    class IRHIResource;
}

namespace Lumina
{
    template<typename TRHIRef>
    class TRGResource
    {
    public:

        virtual ~TRGResource() = default;

        TRHIRef GetRHI() const
        {
            return RHIResource;
        }
    
    private:
        
        TRHIRef RHIResource;
    };


    class FRGBuffer : public TRGResource<FRHIBufferRef>
    {
    public:

        
    };

    class FRGImage : public TRGResource<FRHIBufferRef>
    {
    public:
        
    };
}
