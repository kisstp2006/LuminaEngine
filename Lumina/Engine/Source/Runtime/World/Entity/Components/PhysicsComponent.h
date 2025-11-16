#pragma once

#include "Core/Object/ObjectMacros.h"
#include "Component.h"
#include "PhysicsComponent.generated.h"

namespace Lumina
{

    LUM_ENUM()
    enum class LUMINA_API EBodyType : uint8
    {
        None,
        Static,
        Kinematic,
        Dynamic,
    };    
    
    
    LUM_STRUCT()
    struct LUMINA_API SRigidBodyComponent
    {
        GENERATED_BODY()
        ENTITY_COMPONENT(SRigidBodyComponent)

        LUM_PROPERTY(Editable, Category = "Physics")
        EBodyType BodyType = EBodyType::Dynamic;

        LUM_PROPERTY(Editable, Category = "Physics")
        float Mass = 1.0f;

        LUM_PROPERTY(Editable, Category = "Physics")
        bool bUseGravity = true;

        LUM_PROPERTY(Editable, ClampMin = 0.001f, ClampMax = 1.0f, Category = "Physics")
        float LinearDamping = 0.0f;

        LUM_PROPERTY(Editable, ClampMin = 0.001f, ClampMax = 1.0f, Category = "Physics")
        float AngularDamping = 0.05f;
    };

    LUM_STRUCT()
    struct LUMINA_API SBoxColliderComponent
    {
        GENERATED_BODY()
        ENTITY_COMPONENT(SBoxColliderComponent)

        LUM_PROPERTY(Editable)
        glm::vec3 HalfExtent = glm::vec3(0.5f);

        LUM_PROPERTY(Editable)
        glm::vec3 Offset;
    };

    LUM_STRUCT()
    struct LUMINA_API SSphereColliderComponent
    {
        GENERATED_BODY()
        ENTITY_COMPONENT(SSphereColliderComponent)

        LUM_PROPERTY(Editable)
        float Radius = 0.5f;

        LUM_PROPERTY(Editable)
        glm::vec3 Offset;
    };
}
