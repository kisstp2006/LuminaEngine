#pragma once

#include "Module/API.h"
#include "Containers/Array.h"
#include "Platform/GenericPlatform.h"
#include "World/Entity/Entity.h"


namespace Lumina
{
    struct LUMINA_API SRelationshipComponent
    {
        constexpr static uint8 MaxChildren = 32;
        
        Entity Parent {};
        
        uint8 NumChildren = 0;
        TArray<Entity, MaxChildren> Children {};
    };
}
