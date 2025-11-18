#pragma once
#include "Containers/Array.h"
#include "Renderer/RHIFwd.h"


namespace Lumina
{
    struct FDepthPyramidPass
    {
        TVector<FRHIBindingSetRef> BindingSets;
        TVector<FRHIBindingLayoutRef> BindingLayouts;
    };
}
