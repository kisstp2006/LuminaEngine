#include "UpdateTransformEntitySystem.h"

#include "Core/Object/Class.h"
#include "TaskSystem/TaskSystem.h"
#include "World/Entity/Components/DirtyComponent.h"
#include "World/Entity/Components/RelationshipComponent.h"
#include "World/Entity/Components/RenderComponent.h"
#include "World/Entity/Components/Transformcomponent.h"

namespace Lumina
{
    
    void CUpdateTransformEntitySystem::Initialize()
    {
        
    }

    void CUpdateTransformEntitySystem::Shutdown()
    {
        
    }

    void CUpdateTransformEntitySystem::Update(FSystemContext& SystemContext)
    {
        LUMINA_PROFILE_SCOPE();

        auto Group = SystemContext.CreateGroup<FNeedsTransformUpdate>(entt::get<STransformComponent>);
        
        auto WorkCallable = [&](uint32 Index)
        {
            TFunction<void(entt::entity)> UpdateTransformRecursive;
            UpdateTransformRecursive = [&](entt::entity entity)
            {
                auto& TransformComponent = Group.get<STransformComponent>(entity);

                if (SystemContext.Has<true, SRelationshipComponent>(entity))
                {
                    auto& RelationshipComponent = SystemContext.Get<SRelationshipComponent>(entity);

                    if (RelationshipComponent.Parent.IsValid())
                    {
                        TransformComponent.WorldTransform = RelationshipComponent.Parent.GetComponent<STransformComponent>().WorldTransform * TransformComponent.Transform;
                    }
                    else
                    {
                        TransformComponent.WorldTransform = TransformComponent.Transform;
                    }
                    
                    for (uint8 i = 0; i < RelationshipComponent.NumChildren; ++i)
                    {
                        Entity ChildEntity = RelationshipComponent.Children[i];
                
                        UpdateTransformRecursive(ChildEntity.GetHandle());
                    }
                }
                else
                {
                    TransformComponent.WorldTransform = TransformComponent.Transform;
                }

                
                TransformComponent.CachedMatrix = TransformComponent.WorldTransform.GetMatrix();  
            };

            
            entt::entity entity = Group[Index];
            UpdateTransformRecursive(entity);
        };

        
        // Only schedule tasks if there is a significant amount of transform updates required.
        if (Group.size() > 100)
        {
            Task::ParallelFor((uint32)Group.size(), WorkCallable);
        }
        else
        {
            for (uint32 i = 0; i < (uint32)Group.size(); ++i)
            {
                WorkCallable(i);
            }
        }
        
        SystemContext.Clear<FNeedsTransformUpdate>();
    }
}
