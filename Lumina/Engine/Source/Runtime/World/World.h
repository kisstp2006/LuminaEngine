#pragma once

#include "Core/Object/Object.h"
#include "Core/UpdateContext.h"
#include "World/Entity/Components/CameraComponent.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "Entity/Registry/EntityRegistry.h"
#include "Entity/EntityWorld.h"
#include "Renderer/RenderGraph/RenderGraph.h"
#include "World.generated.h"

namespace Lumina
{
    class IRenderScene;
    struct SRenderComponent;
    class FCameraManager;
    struct FLineBatcherComponent;
    class CEntitySystem;
    class Entity;
}

namespace Lumina
{
    LUM_CLASS()
    class LUMINA_API CWorld : public CObject
    {
        GENERATED_BODY()
        
        friend class FWorldManager;
        friend struct FSystemContext;
        friend struct SRenderComponent;
        
    public:

        CWorld();

        //~ Begin CObject Interface
        void Serialize(FArchive& Ar) override;
        void PreLoad() override;
        void PostLoad() override;
        //~ End CObject Interface


        /**
         * Initializes systems and renderer.
         */
        void InitializeWorld();
        
        /** Handles setting up this world for editor use,
         * returns the entity used during world manipulation
         * */
        Entity SetupEditorWorld();


        /**
         * Called on every update stage and runs systems attached to this world.
         */
        void Update(const FUpdateContext& Context);
        void Paused(const FUpdateContext& Context);
        void Render(FRenderGraph& RenderGraph);

        /**
         * Called to shut down the world, destroys system, components, and entities.
         */
        void ShutdownWorld();

        bool RegisterSystem(CEntitySystem* NewSystem);

        Entity ConstructEntity(const FName& Name, const FTransform& Transform = FTransform());
        
        void CopyEntity(Entity& To, const Entity& From);
        void ReparentEntity(Entity Child, Entity Parent);
        void DestroyEntity(Entity Entity);

        //LUM_DEPRECATED("0.0.1", "Access to the registry has been deprecated")
        FEntityRegistry& GetEntityRegistry() { return EntityWorld.Registry; }

        //LUM_DEPRECATED("0.0.1", "Access to the registry has been deprecated")
        const FEntityRegistry& GetEntityRegistry_Immutable() const { return EntityWorld.Registry; }

        uint32 GetNumEntities() const;
        void SetActiveCamera(Entity InEntity);
        SCameraComponent& GetActiveCamera();
        Entity GetActiveCameraEntity() const;

        double GetWorldDeltaTime() const { return DeltaTime; }
        double GetTimeSinceWorldCreation() const { return TimeSinceCreation; }

        void SetPaused(bool bNewPause) { bPaused = bNewPause; }
        bool IsPaused() const { return bPaused; }

        void SetActive(bool bNewActive) { bActive = bNewActive; }
        bool IsSuspended() const { return !bActive; }

        static CWorld* DuplicateWorldForPIE(CWorld* OwningWorld);

        IRenderScene* GetRenderer() const { return RenderScene; }

        const TVector<CEntitySystem*>& GetSystemsForUpdateStage(EUpdateStage Stage);

        //~ Begin Debug Drawing
        void DrawDebugLine(const glm::vec3& Start, const glm::vec3& End, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f);
        void DrawDebugBox(const glm::vec3& Center, const glm::vec3& Extents, const glm::quat& Rotation, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f);
        void DrawDebugSphere(const glm::vec3& Center, float Radius, const glm::vec4& Color, uint8 Segments = 16, float Thickness = 1.0f, float Duration = 1.0f);
        void DrawDebugCone(const glm::vec3& Apex, const glm::vec3& Direction, float AngleRadians, float Length, const glm::vec4& Color, uint8 Segments = 16, uint8 Stacks = 4, float Thickness = 1.0f, float Duration = 1.0f);
        void DrawFrustum(const glm::mat4& Matrix, float zNear, float zFar, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f);
        void DrawArrow(const glm::vec3& Start, const glm::vec3& Direction, float Length, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f, float HeadSize = 0.2f);
        void DrawViewVolume(const FViewVolume& ViewVolume, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f);
        //~ End Debug Drawing

        void SetIsPlayWorld(bool bValue) { bIsPlayWorld = bValue; }
        FORCEINLINE bool IsPlayWorld() const { return bIsPlayWorld; }

        void SetEntityTransform(Entity Entt, const FTransform& NewTransform);

        void SetSelectedEntity(entt::entity EntityID) { SelectedEntity = EntityID; }
        FORCEINLINE entt::entity GetSelectedEntity() const { return SelectedEntity; }
        
    private:
        
        FLineBatcherComponent& GetOrCreateLineBatcher();
    
    private:
        
        FEntityWorld                                    EntityWorld;
        FLineBatcherComponent*                          LineBatcherComponent = nullptr;

        FCameraManager*                                 CameraManager = nullptr;
        IRenderScene*                                   RenderScene = nullptr;
        
        TVector<CEntitySystem*>                         SystemUpdateList[(int32)EUpdateStage::Max];
        
        eastl::atomic<bool>                             bInitialized{false};
        
        int64                                           WorldIndex = -1;
        double                                          DeltaTime = 0.0;
        double                                          TimeSinceCreation = 0.0;
        uint32                                          bPaused:1=1;
        uint32                                          bActive:1=1;
        uint32                                          bIsPlayWorld:1=0;
        entt::entity                                    SelectedEntity;
        entt::entity                                    ThumbnailCaptureEntity;
        
    };
}
