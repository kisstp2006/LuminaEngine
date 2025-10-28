#include "UpdateTransformEntitySystem.h"

#include "Core/Object/Class.h"
#include "glm/gtx/string_cast.hpp"
#include "TaskSystem/TaskSystem.h"
#include "World/Entity/Components/DirtyComponent.h"
#include "World/Entity/Components/RelationshipComponent.h"
#include "World/Entity/Components/RenderComponent.h"
#include "World/Entity/Components/Transformcomponent.h"

namespace Lumina
{
    
    void CUpdateTransformEntitySystem::Initialize(FSystemContext& SystemContext)
    {
        
    }

    void CUpdateTransformEntitySystem::Shutdown(FSystemContext& SystemContext)
    {
        
    }

    void CUpdateTransformEntitySystem::Update(FSystemContext& SystemContext)
    {
        LUMINA_PROFILE_SCOPE();

        auto SingleView = SystemContext.CreateView<FNeedsTransformUpdate, STransformComponent>(entt::exclude<SRelationshipComponent>);
        auto RelationshipGroup = SystemContext.CreateGroup<FNeedsTransformUpdate, SRelationshipComponent>(entt::get<STransformComponent>);
        
        if (!RelationshipGroup.empty())
        {
            auto WorkCallable = [&](uint32 Index)
            {
                TFunction<void(entt::entity)> UpdateTransformRecursive;
                UpdateTransformRecursive = [&](entt::entity entity)
                {
                    auto& TransformComponent = RelationshipGroup.get<STransformComponent>(entity);
            
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
            
                    if (SystemContext.Has<true, SCameraComponent>(entity))
                    {
                        glm::vec3 UpdatedForward = TransformComponent.WorldTransform.Rotation * FViewVolume::ForwardAxis;
                        glm::vec3 UpdatedUp = TransformComponent.WorldTransform.Rotation * FViewVolume::UpAxis;
                        
                        SCameraComponent& Camera = SystemContext.Get<SCameraComponent>(entity);
                        Camera.SetView(TransformComponent.WorldTransform.Location, UpdatedForward, UpdatedUp);
                    }
                    
                    TransformComponent.CachedMatrix = TransformComponent.WorldTransform.GetMatrix();  
                };
            
                
                entt::entity entity = RelationshipGroup[Index];
                UpdateTransformRecursive(entity);
            };
            
            // Only schedule tasks if there is a significant amount of transform updates required.
            if (RelationshipGroup.size() > 1000)
            {
                Task::ParallelFor((uint32)RelationshipGroup.size(), RelationshipGroup.size() / 8, WorkCallable);
            }
            else
            {
                for (uint32 i = 0; i < (uint32)RelationshipGroup.size(); ++i)
                {
                    WorkCallable(i);
                }
            }
        }

        if (SingleView.size_hint() < 1000)
        {
            SingleView.each([&](auto& TU, STransformComponent& TransformComponent)
            {
                TransformComponent.WorldTransform = TransformComponent.Transform;
                TransformComponent.CachedMatrix = TransformComponent.WorldTransform.GetMatrix();  
            });
        }
        else
        {
            std::vector Entities(SingleView.begin(), SingleView.end());

            auto WorkFunctor = [&](FNeedsTransformUpdate& Update, STransformComponent& Transform)
            {
                Transform.WorldTransform = Transform.Transform;
                Transform.CachedMatrix = Transform.Transform.GetMatrix();
            };
            
            Task::ParallelFor(Entities.size(), 64, [&, SingleView](uint32 Index)
            {
                entt::entity Entity = Entities[Index];
                std::apply(WorkFunctor, SingleView.get(Entity));
            });
            
        }
        
        SystemContext.Clear<FNeedsTransformUpdate>();
    }
}
