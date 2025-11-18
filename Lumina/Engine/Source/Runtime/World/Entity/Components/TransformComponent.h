#pragma once

#include "Component.h"
#include "Core/Math/Transform.h"
#include <glm/glm.hpp>
#include "TransformComponent.generated.h"

namespace Lumina
{
    LUM_STRUCT()
    struct LUMINA_API STransformComponent
    {
        GENERATED_BODY()
        ENTITY_COMPONENT(STransformComponent)
        
        STransformComponent() = default;
        STransformComponent(const FTransform& InTransform)
            : Transform(InTransform)
        {}
        
        STransformComponent(const glm::vec3& InPosition)
            :Transform(InPosition)
        {}
        
        FORCEINLINE FTransform& GetTransform() { return Transform; }
        FORCEINLINE void SetTransform(const FTransform& InTransform) { Transform = InTransform; }

        FORCEINLINE glm::vec3 GetLocation() const    { return Transform.Location; }
        FORCEINLINE glm::quat GetRotation() const    { return Transform.Rotation; }
        FORCEINLINE glm::vec3 GetScale()    const    { return Transform.Scale; }
        FORCEINLINE float MaxScale()    const    { return glm::max(Transform.Scale.x, glm::max(Transform.Scale.y, Transform.Scale.z)); }
        FORCEINLINE glm::mat4 GetMatrix()   const    { return CachedMatrix; }
        
        FORCEINLINE STransformComponent& SetLocation(const glm::vec3& InLocation) 
        { 
            Transform.Location = InLocation;
            return *this;
        }

        FORCEINLINE STransformComponent& SetRotation(const glm::quat& InRotation)
        { 
            Transform.Rotation = InRotation;
            return *this;
        }

        FORCEINLINE STransformComponent& SetRotationFromEuler(const glm::vec3& EulerRotation)
        {
            Transform.Rotation = glm::quat(glm::radians(EulerRotation));
            return *this;
        }

        FORCEINLINE STransformComponent& SetScale(const glm::vec3& InScale) 
        { 
            Transform.Scale = InScale;
            return *this;
        }

        FORCEINLINE glm::vec3 GetRotationAsEuler() const 
        {
            return glm::degrees(glm::eulerAngles(Transform.Rotation));
        }

    public:

        LUM_PROPERTY(Editable, Category = "Transform")
        FTransform Transform;
        
        FTransform WorldTransform = Transform;
        
        glm::mat4 CachedMatrix = WorldTransform.GetMatrix();
    };
    
}
