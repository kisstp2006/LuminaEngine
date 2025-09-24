#pragma once
#include "Containers/Array.h"
#include "Core/Variant/Variant.h"

namespace Lumina
{
    template<typename ... Ts>
    using TSceneRenderCommand = TVariant<Ts...>;

    template<typename ... Ts>
    using TSceneRenderCommandStack = TQueue<TSceneRenderCommand<Ts...>>;


    


    

    
    using FSceneRenderCommandStack = TSceneRenderCommandStack<>;
    
    
}
