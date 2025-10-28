
#include "Entity.h"

#include "Components/DirtyComponent.h"
#include "Components/RelationshipComponent.h"
#include "Core/Object/Class.h"

namespace Lumina
{

    FArchive& Entity::Serialize(FArchive& Ar)
    {
        if (Ar.IsWriting())
        {
            Assert(World.IsValid())
            
            int64 NumComponentsPos = Ar.Tell();
            
            SIZE_T NumComponents = 0;
            Ar << NumComponents;
            
            for (auto [ID, Set] : World->GetEntityRegistry().storage())
            {
                if (Set.contains(GetHandle()))
                {
                    using namespace entt::literals;
                    
                    void* ComponentPointer = Set.value(GetHandle());
                    auto ReturnValue = entt::resolve(Set.type()).invoke("staticstruct"_hs, {});
                    
                    if (void** Type = ReturnValue.try_cast<void*>())
                    {
                        if (CStruct* StructType = *(CStruct**)Type)
                        {
                            FName Name = StructType->GetQualifiedName();
                            Ar << Name;
                            
                            int64 ComponentStart = Ar.Tell();

                            int64 ComponentSize = 0;
                            Ar << ComponentSize;

                            int64 StartOfComponentData = Ar.Tell();
                            
                            StructType->SerializeTaggedProperties(Ar, ComponentPointer);

                            int64 EndOfComponentData = Ar.Tell();

                            ComponentSize = EndOfComponentData - StartOfComponentData;

                            Ar.Seek(ComponentStart);
                            Ar << ComponentSize;
                            Ar.Seek(EndOfComponentData);
                            
                            NumComponents++;
                        }
                    }
                }
            }

            int64 SizeBefore = Ar.Tell();
            Ar.Seek(NumComponentsPos);    
            Ar << NumComponents;
            Ar.Seek(SizeBefore);
            

            if (World->GetEntityRegistry().all_of<SRelationshipComponent>(GetHandle()))
            {
                auto& RelComp = World->GetEntityRegistry().get<SRelationshipComponent>(GetHandle());
                SIZE_T NumChildren = RelComp.NumChildren;
                Ar << NumChildren;

                for (SIZE_T i = 0; i < NumChildren; ++i)
                {
                    RelComp.Children[i].Serialize(Ar);
                }
            }
            else
            {
                SIZE_T NumChildren = 0;
                Ar << NumChildren;
            }
        }
        else if (Ar.IsReading())
        {
            SIZE_T NumComponents = 0;
            Ar << NumComponents;
    
            for (SIZE_T i = 0; i < NumComponents; ++i)
            {
                FName TypeName;
                Ar << TypeName;

                int64 ComponentSize = 0;
                Ar << ComponentSize;

                int64 ComponentStart = Ar.Tell();

                if (CStruct* Struct = FindObject<CStruct>(nullptr, TypeName))
                {
                    using namespace entt::literals;
                    void* RegistryPtr = &World->GetEntityRegistry();
                    entt::hashed_string HashString(Struct->GetName().c_str());
                    if (entt::meta_type Meta = entt::resolve(HashString))
                    {
                        entt::meta_any RetVal = Meta.invoke("addcomponent"_hs, {}, GetHandle(), RegistryPtr);
                        void** Type = RetVal.try_cast<void*>();

                        Struct->SerializeTaggedProperties(Ar, *Type);
                    }
                }

                int64 ComponentEnd = ComponentSize + ComponentStart;
                Ar.Seek(ComponentEnd);
                
            }
    
            SIZE_T NumChildren = 0;
            Ar << NumChildren;
    
            if (NumChildren > 0)
            {
                auto& rel = World->GetEntityRegistry().emplace<SRelationshipComponent>(GetHandle());
    
                for (SIZE_T i = 0; i < NumChildren; ++i)
                {
                    // Create new child entity
                    entt::entity ChildHandle = World->GetEntityRegistry().create();
                    rel.Children[rel.NumChildren] = Entity(ChildHandle, World);
                    rel.NumChildren++;
    
                    Entity ChildEntity(ChildHandle, World);
                    ChildEntity.Serialize(Ar);
    
                    // Patch parent relationship
                    if (World->GetEntityRegistry().all_of<SRelationshipComponent>(ChildHandle))
                    {
                        auto& childRel = World->GetEntityRegistry().get<SRelationshipComponent>(ChildHandle);
                        childRel.Parent = *this;
                    }
                    else
                    {
                        auto& childRel = World->GetEntityRegistry().emplace<SRelationshipComponent>(ChildHandle);
                        childRel.Parent = *this;
                    }
                }
            }

            EmplaceOrReplace<FNeedsTransformUpdate>();

        }
    
        return Ar;
    }
    
    bool Entity::IsChild()
    {
        if (auto* Component = TryGetComponent<SRelationshipComponent>())
        {
            return Component->Parent.IsValid();
        }

        return false;
    }

    Entity Entity::GetParent()
    {
        Assert(IsChild())

        return GetComponent<SRelationshipComponent>().Parent;
    }

    FTransform Entity::GetWorldTransform()
    {
        auto& TransformComp = GetComponent<STransformComponent>();
        return TransformComp.WorldTransform;
    }

    FTransform Entity::GetLocalTransform()
    {
        auto& TransformComp = GetComponent<STransformComponent>();
   
        if (IsChild())
        {
            return TransformComp.Transform;
        }
        else
        {
            return FTransform();
        }
    }
}
