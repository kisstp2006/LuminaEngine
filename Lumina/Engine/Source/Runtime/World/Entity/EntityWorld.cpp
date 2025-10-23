#include "EntityWorld.h"

#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Components/DirtyComponent.h"

namespace Lumina
{
    struct FNeedsTransformUpdate;

    entt::entity FEntityWorld::Create(const FName& Name, const FTransform& Transform)
    {
        entt::entity NewEntity = Registry.create();
        FString StringName(Name.c_str());
        StringName += "_" + eastl::to_string((int)NewEntity);
        
        Registry.emplace<SNameComponent>(NewEntity).Name = StringName;
        Registry.emplace<STransformComponent>(NewEntity).Transform = Transform;
        Registry.emplace_or_replace<FNeedsTransformUpdate>(NewEntity);

        return NewEntity;
    }

    entt::entity FEntityWorld::Create()
    {
        entt::entity NewEntity = Registry.create();
        FString StringName("Entity_" + eastl::to_string((int)NewEntity));
        
        Registry.emplace<SNameComponent>(NewEntity).Name = StringName;
        Registry.emplace<STransformComponent>(NewEntity);
        Registry.emplace_or_replace<FNeedsTransformUpdate>(NewEntity);

        return NewEntity;
    }
}
