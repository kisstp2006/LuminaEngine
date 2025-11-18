#include "JoltPhysicsScene.h"

#include "JoltPhysics.h"
#include "Core/Profiler/Profile.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "World/World.h"
#include "World/Entity/Components/DirtyComponent.h"
#include "World/Entity/Components/PhysicsComponent.h"
#include "World/Entity/Components/TransformComponent.h"

using namespace JPH::literals;

namespace Lumina::Physics
{

    constexpr JPH::EMotionType ToJoltMotionType(EBodyType BodyType)
    {
        switch (BodyType)
        {
            case EBodyType::None:       return JPH::EMotionType::Static;
            case EBodyType::Static:     return JPH::EMotionType::Static;
            case EBodyType::Kinematic:  return JPH::EMotionType::Kinematic;
            case EBodyType::Dynamic:    return JPH::EMotionType::Dynamic;
        }

        LUMINA_NO_ENTRY();
    }

    constexpr JPH::ObjectLayer ToJoltObjectType(EBodyType BodyType)
    {
        if (BodyType == EBodyType::Dynamic || BodyType == EBodyType::Kinematic)
        {
            return Layers::MOVING;
        }

        return Layers::NON_MOVING;
    }
    
    class FObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::NON_MOVING:    return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:        return true;
            default:
                JPH_ASSERT(false);
                return false;
            }
        }
    };

    class FObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            switch (inObject1)
            {
            case Layers::NON_MOVING: return inObject2 == Layers::MOVING; // Non moving only collides with moving
            case Layers::MOVING: return true; // Moving collides with everything
            default:
                JPH_ASSERT(false);
                return false;
            }
        }
    };

    static FObjectLayerPairFilterImpl GObjectVsObjectLayerFilter;
    static FObjectVsBroadPhaseLayerFilterImpl GObjectVsBroadPhaseLayerFilter;


    void JoltContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        
    }

    void JoltContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        ContactListener::OnContactPersisted(inBody1, inBody2, inManifold, ioSettings);
    }

    void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
    {
        ContactListener::OnContactRemoved(inSubShapePair);
    }

    void JoltContactListener::OverrideFrictionAndRestitution(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
    }

    void JoltContactListener::GetFrictionAndRestitution(const JPH::Body& inBody, const JPH::SubShapeID& inSubShapeID, float& outFriction, float& outRestitution) const
    {
    }

    FJoltPhysicsScene::FJoltPhysicsScene(CWorld* InWorld)
        : World(InWorld)
    {
        JoltSystem = MakeUniquePtr<JPH::PhysicsSystem>();
        JoltInterfaceLayer = MakeUniquePtr<FLayerInterfaceImpl>();
        
        JoltSystem->Init(20240, 0, 65536, 20240, *JoltInterfaceLayer, GObjectVsBroadPhaseLayerFilter, GObjectVsObjectLayerFilter);
        JoltSystem->SetGravity(JPH::Vec3Arg(0.0f, -9.81f, 0.0f));

        JPH::PhysicsSettings JoltSettings;
        JoltSystem->SetPhysicsSettings(JoltSettings);
    }

    FJoltPhysicsScene::~FJoltPhysicsScene()
    {
        JoltInterfaceLayer.reset();
        JoltSystem.reset();
    }

    void FJoltPhysicsScene::PreUpdate()
    {
    }

    void FJoltPhysicsScene::PostUpdate()
    {
        LUMINA_PROFILE_SCOPE();

        entt::registry& Registry = World->GetEntityRegistry();
        auto View = Registry.view<FJoltBodyComponent, STransformComponent>();
        
        View.each([&](entt::entity EntityID, FJoltBodyComponent& Body, STransformComponent& TransformComponent)
        {
            if (!Body.Body->IsActive())
            {
                return;
            }
            
            auto Pos = Body.Body->GetPosition();
            auto Rot = Body.Body->GetRotation();

            TransformComponent.SetLocation(glm::vec3(Pos.GetX(), Pos.GetY(), Pos.GetZ()));
            TransformComponent.SetRotation(glm::quat(Rot.GetW(), Rot.GetX(), Rot.GetY(), Rot.GetZ()));
            
            Registry.emplace_or_replace<FNeedsTransformUpdate>(EntityID);
        });
    }

    void FJoltPhysicsScene::Update(double DeltaTime)
    {
        LUMINA_PROFILE_SCOPE();
        
        if (Accumulator > FixedTimeStep)
        {
            Accumulator = 0.0f;
        }

        Accumulator += DeltaTime;
        if (Accumulator < FixedTimeStep)
        {
            CollisionSteps = 0;
            return;
        }

        CollisionSteps = (uint32)(Accumulator / FixedTimeStep);
        Accumulator -= (float)CollisionSteps * FixedTimeStep;

        if (CollisionSteps > 0)
        {
            PreUpdate();
            JoltSystem->Update(FixedTimeStep, CollisionSteps, FJoltPhysicsContext::GetAllocator(), FJoltPhysicsContext::GetThreadPool());
            PostUpdate();
        }
    }

    void FJoltPhysicsScene::OnWorldSimulate()
    {
        entt::registry& Registry = World->GetEntityRegistry();
        auto View = Registry.view<SRigidBodyComponent, STransformComponent>(entt::exclude<FJoltBodyComponent>);
        
        View.each([&] (entt::entity EntityID, SRigidBodyComponent&, STransformComponent&)
        {
            OnRigidBodyComponentConstructed(Registry, EntityID);
        });

        JoltSystem->OptimizeBroadPhase();

        Registry.on_construct<SRigidBodyComponent>().connect<&FJoltPhysicsScene::OnRigidBodyComponentConstructed>(this);
        Registry.on_destroy<SRigidBodyComponent>().connect<&FJoltPhysicsScene::OnRigidBodyComponentDestroyed>(this);
    }

    void FJoltPhysicsScene::OnWorldStopSimulate()
    {
        entt::registry& Registry = World->GetEntityRegistry();

        Registry.on_construct<SRigidBodyComponent>().disconnect<&FJoltPhysicsScene::OnRigidBodyComponentConstructed>(this);
        Registry.on_destroy<SRigidBodyComponent>().disconnect<&FJoltPhysicsScene::OnRigidBodyComponentDestroyed>(this);

        auto View = Registry.view<SRigidBodyComponent, FJoltBodyComponent>();
        View.each([&] (entt::entity EntityID, SRigidBodyComponent&, FJoltBodyComponent&)
        {
           OnRigidBodyComponentDestroyed(Registry, EntityID); 
        });
    }

    void FJoltPhysicsScene::OnRigidBodyComponentConstructed(entt::registry& Registry, entt::entity EntityID)
    {
        if (!Registry.any_of<SSphereColliderComponent, SBoxColliderComponent>(EntityID))
        {
            LOG_ERROR("Entity {} attempted to construct a rigid body without a collider!");
            return;
        }

        SRigidBodyComponent& RigidBodyComponent = Registry.get<SRigidBodyComponent>(EntityID);
        STransformComponent& TransformComponent = Registry.get<STransformComponent>(EntityID);
        
        JPH::ShapeRefC Shape;

        if (Registry.all_of<SBoxColliderComponent>(EntityID))
        {
            auto& BC = Registry.get<SBoxColliderComponent>(EntityID);
            JPH::BoxShapeSettings Settings(JPH::RVec3(BC.HalfExtent.x * TransformComponent.GetScale().x, BC.HalfExtent.y * TransformComponent.GetScale().y, BC.HalfExtent.z * TransformComponent.GetScale().z));
            Settings.SetEmbedded();
            Shape = Settings.Create().Get();
        }
        else if (Registry.all_of<SSphereColliderComponent>(EntityID))
        {
            auto& SC = Registry.get<SSphereColliderComponent>(EntityID);
            JPH::SphereShapeSettings Settings(SC.Radius * TransformComponent.MaxScale());
            Settings.SetEmbedded();
            Shape = Settings.Create().Get();
        }

        JPH::ObjectLayer Layer = ToJoltObjectType(RigidBodyComponent.BodyType);
        JPH::EMotionType MotionType = ToJoltMotionType(RigidBodyComponent.BodyType);

        glm::quat NormalizedRot = TransformComponent.GetRotation();
        glm::vec3 Position = TransformComponent.GetLocation();
        JPH::Quat JoltQuat = JPH::Quat(NormalizedRot.x, NormalizedRot.y, NormalizedRot.z, NormalizedRot.w);

        JPH::BodyCreationSettings Settings(
            Shape,
            JPH::Vec3(Position.x, Position.y, Position.z),
            JoltQuat,
            MotionType,
            Layer);

        Settings.mRestitution = 0.5f;
        Settings.mFriction = 0.3f;
        Settings.mAngularDamping = RigidBodyComponent.AngularDamping;
        Settings.mLinearDamping = RigidBodyComponent.LinearDamping;

        JPH::BodyInterface& BodyInterface = JoltSystem->GetBodyInterface();
        
        JPH::Body* Body = BodyInterface.CreateBody(Settings);
        BodyInterface.AddBody(Body->GetID(), JPH::EActivation::Activate);
        Registry.emplace<FJoltBodyComponent>(EntityID, Body);
    }

    void FJoltPhysicsScene::OnRigidBodyComponentDestroyed(entt::registry& Registry, entt::entity EntityID)
    {
        if (FJoltBodyComponent* JoltBodyComponent = Registry.try_get<FJoltBodyComponent>(EntityID))
        {
            JPH::BodyInterface& BodyInterface = JoltSystem->GetBodyInterface();
        
            BodyInterface.RemoveBody(JoltBodyComponent->Body->GetID());
            BodyInterface.DestroyBody(JoltBodyComponent->Body->GetID());
            Registry.remove<FJoltBodyComponent>(EntityID);
        }
    }

    void FJoltPhysicsScene::OnColliderComponentAdded(entt::registry& Registry, entt::entity EntityID)
    {
    }

    void FJoltPhysicsScene::OnColliderComponentRemoved(entt::registry& Registry, entt::entity EntityID)
    {
    }
}
