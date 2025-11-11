#include "World.h"

#include "WorldManager.h"
#include "Core/Engine/Engine.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Serialization/MemoryArchiver.h"
#include "Core/Serialization/ObjectArchiver.h"
#include "EASTL/sort.h"
#include "Entity/Components/DirtyComponent.h"
#include "Entity/Components/EditorComponent.h"
#include "Entity/Components/LineBatcherComponent.h"
#include "Entity/Components/VelocityComponent.h"
#include "Entity/Systems/UpdateTransformEntitySystem.h"
#include "glm/gtx/matrix_decompose.hpp"
#include "Scene/RenderScene/Forward/ForwardRenderScene.h"
#include "Subsystems/FCameraManager.h"
#include "TaskSystem/TaskSystem.h"
#include "World/Entity/Components/RelationshipComponent.h"
#include "World/Scene/RenderScene/Deferred/DeferredRenderScene.h"
#include "World/entity/systems/EntitySystem.h"

namespace Lumina
{
    CWorld::CWorld()
    {
        SelectedEntity = entt::null;
    }

    void CWorld::Serialize(FArchive& Ar)
    {
        CObject::Serialize(Ar);
        
        if (Ar.IsWriting())
        {
            GetEntityRegistry().compact<>();
            auto View = GetEntityRegistry().view<entt::entity>(entt::exclude<SEditorComponent>);

            TVector<entt::entity> Parents;
            Parents.reserve(View.size_hint());
            
            for (entt::entity entity : View)
            {
                // We only want to serialize top-level entities here, parents will serialize their children.
                if (GetEntityRegistry().all_of<SRelationshipComponent>(entity))
                {
                    auto& RelationshipComponent = GetEntityRegistry().get<SRelationshipComponent>(entity);
                    if (RelationshipComponent.Parent.IsValid())
                    {
                        continue;
                    }
                }
                
                Parents.emplace_back(entity);
            }

            uint32 NumParents = (uint32)Parents.size();
            Ar << NumParents;

            for (entt::entity Parent : Parents)
            {
                int64 EntityStart = Ar.Tell();

                int64 EntitySize = 0;
                Ar << EntitySize;

                Entity TopLevelEntity(Parent, this);
                int64 StartOfEntityData = Ar.Tell();
                TopLevelEntity.Serialize(Ar);
                int64 EndOfEntityData = Ar.Tell();

                EntitySize = EndOfEntityData - StartOfEntityData;

                Ar.Seek(EntityStart);
                Ar << EntitySize;
                Ar.Seek(EndOfEntityData);
            }
        }
        else if (Ar.IsReading())
        {
            uint32 NumParents = 0;
            Ar << NumParents;
            
            for (uint32 i = 0; i < NumParents; ++i)
            {
                int64 EntitySize = 0;
                Ar << EntitySize;

                int64 EntityStart = Ar.Tell();
                
                Entity NewEntity(GetEntityRegistry().create(), this);
                NewEntity.Serialize(Ar);

                int64 EntityEnd = EntityStart + EntitySize;
                Ar.Seek(EntityEnd);
            }
        }
    }

    void CWorld::PreLoad()
    {
        InitializeWorld();
    }

    void CWorld::PostLoad()
    {
        //...
    }

    void CWorld::InitializeWorld()
    {
        if (bInitialized.load(eastl::memory_order_acquire))
        {
            return;
        }
        
        CameraManager = Memory::New<FCameraManager>();
        SetupEditorWorld();

        RenderScene = Memory::New<FForwardRenderScene>(this);
        RenderScene->Init();

        TVector<TObjectPtr<CEntitySystem>> Systems;
        CEntitySystemRegistry::Get().GetRegisteredSystems(Systems);
        
        for (CEntitySystem* System : Systems)
        {
            if (System->GetRequiredUpdatePriorities())
            {
                CEntitySystem* DuplicateSystem = NewObject<CEntitySystem>(System->GetClass());
                RegisterSystem(DuplicateSystem);
            }
        }

        bInitialized.store(true, eastl::memory_order_relaxed);
    }
    
    Entity CWorld::SetupEditorWorld()
    {
        Entity EditorEntity = ConstructEntity("Editor Entity");
        EditorEntity.Emplace<SCameraComponent>();
        EditorEntity.Emplace<SEditorComponent>();
        EditorEntity.Emplace<SVelocityComponent>().Speed = 50.0f;
        EditorEntity.Emplace<SHiddenComponent>();
        EditorEntity.GetComponent<STransformComponent>().SetLocation(glm::vec3(0.0f, 0.0f, 1.5f));

        SetActiveCamera(EditorEntity);

        Entity ThumbnailEntity = ConstructEntity("Thumbnail Entity");
        ThumbnailEntity.Emplace<SCameraComponent>().SetAspectRatio(1.0f);
        ThumbnailEntity.Emplace<SEditorComponent>();
        ThumbnailEntity.Emplace<SVelocityComponent>().Speed = 50.0f;
        ThumbnailEntity.Emplace<SHiddenComponent>();
        ThumbnailEntity.GetComponent<STransformComponent>().SetLocation(glm::vec3(0.0f, 0.0f, 1.5f));

        return EditorEntity;
    }

    void CWorld::Update(const FUpdateContext& Context)
    {
        LUMINA_PROFILE_SCOPE();

        const EUpdateStage Stage = Context.GetUpdateStage();
        
        if (Stage == EUpdateStage::FrameStart)
        {
            DeltaTime = Context.GetDeltaTime();
            TimeSinceCreation += DeltaTime;
        }
        
        FSystemContext SystemContext(this);
        
        auto& SystemVector = SystemUpdateList[(uint32)Stage];
        for(CEntitySystem* System : SystemVector)
        {
            System->Update(SystemContext);
        }
    }

    void CWorld::Paused(const FUpdateContext& Context)
    {
        LUMINA_PROFILE_SCOPE();

        DeltaTime = Context.GetDeltaTime();
        TimeSinceCreation += DeltaTime;
        
        FSystemContext SystemContext(this);
        
        auto& SystemVector = SystemUpdateList[(uint32)EUpdateStage::Paused];
        for(CEntitySystem* System : SystemVector)
        {
            System->Update(SystemContext);
        }
    }

    void CWorld::Render(FRenderGraph& RenderGraph)
    {
        LUMINA_PROFILE_SCOPE();

        SCameraComponent& CameraComponent = GetActiveCamera();
        STransformComponent& CameraTransform = GetActiveCameraEntity().GetComponent<STransformComponent>();

        glm::vec3 UpdatedForward = CameraTransform.Transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 UpdatedUp      = CameraTransform.Transform.Rotation * glm::vec3(0.0f, 1.0f,  0.0f);
        
        CameraComponent.SetView(CameraTransform.Transform.Location, UpdatedForward, UpdatedUp);
        
        RenderScene->RenderScene(RenderGraph, CameraComponent.GetViewVolume());
    }

    void CWorld::ShutdownWorld()
    {
        FSystemContext SystemContext(this);

        // Collect unique systems across all stages
        TVector<CEntitySystem*> UniqueSystems;
    
        for (uint8 i = 0; i < (uint8)EUpdateStage::Max; i++)
        {
            for (CEntitySystem* System : SystemUpdateList[i])
            {
                if (eastl::find(UniqueSystems.begin(), UniqueSystems.end(), System) == UniqueSystems.end())
                {
                    UniqueSystems.push_back(System);
                }
            }
        
            SystemUpdateList[i].clear();
        }

        for (CEntitySystem* System : UniqueSystems)
        {
            System->Shutdown(SystemContext);
            System->ConditionalBeginDestroy();
        }

        RenderScene->Shutdown();
        Memory::Delete(RenderScene);
        
        Memory::Delete(CameraManager);
    }

    bool CWorld::RegisterSystem(CEntitySystem* NewSystem)
    {
        Assert(NewSystem != nullptr)
    
        FSystemContext SystemContext(this);

        if (bIsPlayWorld)
        {
            NewSystem->Initialize(SystemContext);
        }
        else
        {
            NewSystem->InitializeEditor(SystemContext);
        }

        bool StagesModified[(uint8)EUpdateStage::Max] = {};

        for (uint8 i = 0; i < (uint8)EUpdateStage::Max; ++i)
        {
            if (NewSystem->GetRequiredUpdatePriorities()->IsStageEnabled((EUpdateStage)i))
            {
                SystemUpdateList[i].push_back(NewSystem);
                StagesModified[i] = true;
            }
        }

        for (uint8 i = 0; i < (uint8)EUpdateStage::Max; ++i)
        {
            if (!StagesModified[i])
            {
                continue;
            }

            auto Predicate = [i](CEntitySystem* A, CEntitySystem* B)
            {
                const uint8 PriorityA = A->GetRequiredUpdatePriorities()->GetPriorityForStage((EUpdateStage)i);
                const uint8 PriorityB = B->GetRequiredUpdatePriorities()->GetPriorityForStage((EUpdateStage)i);
                return PriorityA > PriorityB;
            };

            eastl::sort(SystemUpdateList[i].begin(), SystemUpdateList[i].end(), Predicate);
        }

        return true;
    }

    Entity CWorld::ConstructEntity(const FName& Name, const FTransform& Transform)
    {
        entt::entity NewEntity = GetEntityRegistry().create();

        FString StringName(Name.c_str());
        StringName += "_" + eastl::to_string((int)NewEntity);
        
        GetEntityRegistry().emplace<SNameComponent>(NewEntity).Name = StringName;
        GetEntityRegistry().emplace<STransformComponent>(NewEntity).Transform = Transform;
        GetEntityRegistry().emplace_or_replace<FNeedsTransformUpdate>(NewEntity);
        
        return Entity(NewEntity, this);
    }
    
    void CWorld::CopyEntity(Entity& To, const Entity& From)
    {
        entt::entity NewEntity = GetEntityRegistry().create();
        To = Entity(NewEntity, this);
        
        for (auto [id, storage]: GetEntityRegistry().storage())
        {
            if(storage.contains(From.GetHandle()))
            {
                storage.push(To.GetHandle(), storage.value(From.GetHandle()));
            }
        }

        FString OldName = From.GetConstComponent<SNameComponent>().Name.ToString();

        FString BaseName = OldName;
        size_t Pos = OldName.find_last_of('_');
        if (Pos != FString::npos && Pos + 1 < OldName.size())
        {
            BaseName = OldName.substr(0, Pos);
        }

        FString NewName = BaseName + "_" +  eastl::to_string((uint64)NewEntity);
        To.GetComponent<SNameComponent>().Name = NewName;
    }

    void CWorld::ReparentEntity(Entity Child, Entity Parent)
    {
        if (Child.GetHandle() == Parent.GetHandle())
        {
            LOG_ERROR("Cannot parent an entity to itself!");
            return;
        }
    
        glm::mat4 ChildWorldMatrix = Child.GetWorldTransform().GetMatrix();
    
        glm::mat4 ParentWorldMatrix = glm::mat4(1.0f);
        if (Parent.IsValid())
        {
            ParentWorldMatrix = Parent.GetWorldTransform().GetMatrix();
        }
    
        glm::mat4 NewLocalMatrix = glm::inverse(ParentWorldMatrix) * ChildWorldMatrix;
    
        glm::vec3 Translation, Scale, Skew;
        glm::quat Rotation;
        glm::vec4 Perspective;
    
        glm::decompose(NewLocalMatrix, Scale, Rotation, Translation, Skew, Perspective);
    
        if (Child.HasComponent<STransformComponent>())
        {
            FTransform NewTransform;
            NewTransform.Location = Translation;
            NewTransform.Rotation = Rotation;
            NewTransform.Scale = Scale;
            
            SetEntityTransform(Child, NewTransform);
            
        }
    
        SRelationshipComponent& ParentRelationshipComponent = Parent.GetOrAddComponent<SRelationshipComponent>();
        if (ParentRelationshipComponent.NumChildren >= SRelationshipComponent::MaxChildren)
        {
            LOG_ERROR("Parent has reached its max children");
            return;
        }
    
        SRelationshipComponent& ChildRelationshipComponent = Child.GetOrAddComponent<SRelationshipComponent>();
        if (ChildRelationshipComponent.Parent.IsValid())
        {
            if (SRelationshipComponent* ToRemove = ChildRelationshipComponent.Parent.TryGetComponent<SRelationshipComponent>())
            {
                for (SIZE_T i = 0; i < ToRemove->NumChildren; ++i)
                {
                    if (ToRemove->Children[i] == Child)
                    {
                        for (SIZE_T j = i; j < ToRemove->NumChildren - 1; ++j)
                        {
                            ToRemove->Children[j] = ToRemove->Children[j + 1];
                        }
    
                        --ToRemove->NumChildren;
                        break;
                    }
                }
            }
        }
    
        ParentRelationshipComponent.Children[ParentRelationshipComponent.NumChildren++] = Child;
        ChildRelationshipComponent.Parent = Parent;
    }

    void CWorld::DestroyEntity(Entity Entity)
    {
        Assert(Entity.IsValid())
        GetEntityRegistry().destroy(Entity);
    }

    void CWorld::SetActiveCamera(Entity InEntity)
    {
        CameraManager->SetActiveCamera(InEntity);
    }

    SCameraComponent& CWorld::GetActiveCamera()
    {
        return CameraManager->GetCameraComponent();
    }

    Entity CWorld::GetActiveCameraEntity() const
    {
        return CameraManager->GetActiveCameraEntity();
    }

    CWorld* CWorld::DuplicateWorldForPIE(CWorld* OwningWorld)
    {
        CPackage* OuterPackage = OwningWorld->GetPackage();
        if (OuterPackage == nullptr)
        {
            return nullptr;
        }

        TVector<uint8> Data;
        {
            FMemoryWriter Writer(Data);
            FObjectProxyArchiver Ar(Writer, true);

            OwningWorld->Serialize(Ar);
        }
        
        FMemoryReader Reader(Data);
        FObjectProxyArchiver Ar(Reader, true);
        
        CWorld* PIEWorld = NewObject<CWorld>();
        PIEWorld->SetIsPlayWorld(true);

        PIEWorld->PreLoad();
        PIEWorld->Serialize(Ar);
        PIEWorld->PostLoad();
        
        return PIEWorld;
    }

    const TVector<CEntitySystem*>& CWorld::GetSystemsForUpdateStage(EUpdateStage Stage)
    {
        return SystemUpdateList[uint32(Stage)];
    }
    
    void CWorld::DrawDebugLine(const glm::vec3& Start, const glm::vec3& End, const glm::vec4& Color, float Thickness, float Duration)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawLine(Start, End, Color, Thickness, Duration);
    }

    void CWorld::DrawDebugBox(const glm::vec3& Center, const glm::vec3& Extents, const glm::quat& Rotation, const glm::vec4& Color, float Thickness, float Duration)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawBox(Center, Extents, Rotation, Color, Thickness, Duration);
    }

    void CWorld::DrawDebugSphere(const glm::vec3& Center, float Radius, const glm::vec4& Color, uint8 Segments, float Thickness, float Duration)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawSphere(Center, Radius, Color, Segments, Thickness, Duration);
    }

    void CWorld::DrawDebugCone(const glm::vec3& Apex, const glm::vec3& Direction, float AngleRadians, float Length, const glm::vec4& Color, uint8 Segments, uint8 Stacks, float Thickness, float Duration)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawCone(Apex, Direction, AngleRadians, Length, Color, Segments, Stacks, Thickness, Duration);
    }

    void CWorld::DrawFrustum(const glm::mat4& Matrix, float zNear, float zFar, const glm::vec4& Color, float Thickness, float Duration)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawFrustum(Matrix, zNear, zFar, Color, Thickness, Duration);
    }

    void CWorld::DrawArrow(const glm::vec3& Start, const glm::vec3& Direction, float Length, const glm::vec4& Color, float Thickness, float Duration, float HeadSize)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawArrow(Start, Direction, Length, Color, Thickness, Duration, HeadSize);
    }

    void CWorld::DrawViewVolume(const FViewVolume& ViewVolume, const glm::vec4& Color, float Thickness, float Duration)
    {
        FLineBatcherComponent& Batcher = GetOrCreateLineBatcher();
        Batcher.DrawViewVolume(ViewVolume, Color, Thickness, Duration);
    }

    void CWorld::SetEntityTransform(Entity Entt, const FTransform& NewTransform)
    {
        Entt.EmplaceOrReplace<STransformComponent>(NewTransform);
        Entt.EmplaceOrReplace<FNeedsTransformUpdate>();
    }

    FLineBatcherComponent& CWorld::GetOrCreateLineBatcher()
    {
        if (LineBatcherComponent)
        {
            return *LineBatcherComponent; 
        }
        
        Entity LineBatcherEntity = ConstructEntity("LineBatcher", FTransform());
        LineBatcherEntity.Emplace<SHiddenComponent>();
        LineBatcherEntity.Emplace<SEditorComponent>();
        LineBatcherComponent = &LineBatcherEntity.Emplace<FLineBatcherComponent>();
        
        return *LineBatcherComponent;
    }
}
