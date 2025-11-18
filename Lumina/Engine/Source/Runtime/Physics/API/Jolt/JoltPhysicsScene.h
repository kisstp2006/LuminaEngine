#pragma once
#include "entt/entt.hpp"
#include "Memory/SmartPtr.h"
#include "Physics/PhysicsScene.h"
#include "Jolt/Jolt.h"
#include "Jolt/Physics/PhysicsSystem.h"


namespace Lumina
{
    class CWorld;
}

namespace Lumina::Physics
{

    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };
    
    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr uint32 NUM_LAYERS(2);
    };

    	class JoltContactListener : public JPH::ContactListener
	{
	public:
		JoltContactListener(const JPH::BodyLockInterfaceNoLock* InBodyLockInterface)
			: BodyLockInterface(InBodyLockInterface)
		{ }

		/// Called after detecting a collision between a body pair, but before calling OnContactAdded and before adding the contact constraint.
		/// If the function returns false, the contact will not be added and any other contacts between this body pair will not be processed.
		/// This function will only be called once per PhysicsSystem::Update per body pair and may not be called again the next update
		/// if a contact persists and no new contact pairs between sub shapes are found.
		/// This is a rather expensive time to reject a contact point since a lot of the collision detection has happened already, make sure you
		/// filter out the majority of undesired body pairs through the ObjectLayerPairFilter that is registered on the PhysicsSystem.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// The order of body 1 and 2 is undefined, but when one of the two bodies is dynamic it will be body 1
		virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::CollideShapeResult& inCollisionResult) { return JPH::ValidateResult::AcceptAllContactsForThisBodyPair; }

		/// Called whenever a new contact point is detected.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
		/// Note that only active bodies will report contacts, as soon as a body goes to sleep the contacts between that body and all other
		/// bodies will receive an OnContactRemoved callback, if this is the case then Body::IsActive() will return false during the callback.
		/// When contacts are added, the constraint solver has not run yet, so the collision impulse is unknown at that point.
		/// The velocities of inBody1 and inBody2 are the velocities before the contact has been resolved, so you can use this to
		/// estimate the collision impulse to e.g. determine the volume of the impact sound to play.
		virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);

		/// Called whenever a contact is detected that was also detected last update.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
		virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);

		/// Called whenever a contact was detected last update but is not detected anymore.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// Note that we're using BodyID's since the bodies may have been removed at the time of callback.
		/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair);
    	
	private:
		void OverrideFrictionAndRestitution(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);
		void GetFrictionAndRestitution(const JPH::Body& inBody, const JPH::SubShapeID& inSubShapeID, float& outFriction, float& outRestitution) const;

	private:
		const JPH::BodyLockInterfaceNoLock* BodyLockInterface = nullptr;
	};
    
    class FLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        FLayerInterfaceImpl()
        {
            // Create a mapping table from object to broad phase layer
            ObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            ObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        virtual uint32 GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return ObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char *			GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer)
            {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
            default:													    JPH_ASSERT(false); return "INVALID";
            }
        }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer					ObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    struct LUMINA_API FJoltBodyComponent
    {
        JPH::Body* Body = nullptr;
    };
    
    class FJoltPhysicsScene : public IPhysicsScene
    {
    public:

        FJoltPhysicsScene(CWorld* InWorld);
        ~FJoltPhysicsScene() override;


        void PreUpdate() override;
        void Update(double DeltaTime) override;
        void PostUpdate() override;
        void OnWorldSimulate() override;
        void OnWorldStopSimulate() override;

    	void OnRigidBodyComponentConstructed(entt::registry& Registry, entt::entity EntityID);
    	void OnRigidBodyComponentDestroyed(entt::registry& Registry, entt::entity EntityID);
    	void OnColliderComponentAdded(entt::registry& Registry, entt::entity EntityID);
    	void OnColliderComponentRemoved(entt::registry& Registry, entt::entity EntityID);

    

    private:
        
        TUniquePtr<JPH::PhysicsSystem> JoltSystem;
        TUniquePtr<FLayerInterfaceImpl> JoltInterfaceLayer;
        CWorld* World;


        float FixedTimeStep = 1.0f / 60.0f;
        float Accumulator = 0.0f;
        int CollisionSteps = 1;
    
    };
}
