#pragma once
#include "RenderContext.h"
#include "RenderManager.h"
#include "RenderResource.h"
#include "RHIGlobals.h"


namespace Lumina
{
    template<typename InitializerType, typename RHIRefType>
    class TStaticRHIRef
    {
    public:

        static RHIRefType GetRHI()
        {
            return InitializerType::CreateRHI();
        }
         
    };
    

    template<bool Filter            = true,
    bool MipFilter                  = true,
    ESamplerAddressMode AddressU    = ESamplerAddressMode::Clamp,
    ESamplerAddressMode AddressV    = ESamplerAddressMode::Clamp,
    ESamplerAddressMode AddressW    = ESamplerAddressMode::Clamp,
    ESamplerReductionType Reduction = ESamplerReductionType::Standard>
    class TStaticRHISampler : public TStaticRHIRef<TStaticRHISampler<Filter, MipFilter, AddressU, AddressV, AddressW, Reduction>, FRHISamplerRef>
    {
    public:

        static FRHISamplerRef CreateRHI()
        {
            FSamplerDesc Desc;
            Desc.SetAllFilters(Filter);
            Desc.SetMipFilter(MipFilter);
            Desc.SetReductionType(Reduction);
            Desc.AddressU = AddressU;
            Desc.AddressV = AddressV;
            Desc.AddressW = AddressW;
            Desc.BorderColor = FColor::White;

            return GRenderContext->CreateSampler(Desc);
        }
    };
    
}
