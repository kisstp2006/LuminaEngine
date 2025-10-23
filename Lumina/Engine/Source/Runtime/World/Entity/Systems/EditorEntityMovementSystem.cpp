#include "EditorEntityMovementSystem.h"
#include "Core/Application/Application.h"
#include "Core/Windows/Window.h"
#include "Events/MouseCodes.h"
#include "GLFW/glfw3.h"
#include "Input/Input.h"
#include "Input/InputSubsystem.h"
#include "World/Entity/Entity.h"
#include "World/Entity/Components/CameraComponent.h"
#include "World/Entity/Components/EditorComponent.h"
#include "World/Entity/Components/VelocityComponent.h"
#include "world/Entity/Registry/EntityRegistry.h"

namespace Lumina
{
    void CEditorEntityMovementSystem::Initialize(FSystemContext& SystemContext)
    {
        
    }

    void CEditorEntityMovementSystem::Shutdown(FSystemContext& SystemContext)
    {
        
    }

    void CEditorEntityMovementSystem::Update(FSystemContext& SystemContext)
    {
        LUMINA_PROFILE_SCOPE();
        
        double DeltaTime = SystemContext.GetDeltaTime();
        auto View = SystemContext.CreateView<STransformComponent, SEditorComponent, SCameraComponent, SVelocityComponent>();
        
        for (entt::entity EditorEntity : View)
        {
            STransformComponent& Transform      = View.get<STransformComponent>(EditorEntity);
            SCameraComponent& Camera            = View.get<SCameraComponent>(EditorEntity);
            SEditorComponent& Editor            = View.get<SEditorComponent>(EditorEntity);
            SVelocityComponent& Velocity        = View.get<SVelocityComponent>(EditorEntity);
    
            if (!Editor.bEnabled)
            {
                continue;
            }
    
            glm::vec3 Forward   = Transform.Transform.Rotation * FViewVolume::ForwardAxis * -1.0f;
            glm::vec3 Right     = Transform.Transform.Rotation * FViewVolume::RightAxis;
            glm::vec3 Up        = Transform.Transform.Rotation * FViewVolume::UpAxis;
    
            float Speed = Velocity.Speed;
            if (Input::IsKeyPressed(Key::LeftShift))
            {
                Speed *= 10.0f;
            }
    
            glm::vec3 Acceleration(0.0f);
    
            if (Input::IsKeyPressed(Key::W)) Acceleration += Forward;   // W = forward (+Z)
            if (Input::IsKeyPressed(Key::S)) Acceleration -= Forward;   // S = backward (-Z)
            if (Input::IsKeyPressed(Key::D)) Acceleration += Right;     // D = right (+X)
            if (Input::IsKeyPressed(Key::A)) Acceleration -= Right;     // A = left (-X)
            if (Input::IsKeyPressed(Key::E)) Acceleration += Up;        // E = up (+Y)
            if (Input::IsKeyPressed(Key::Q)) Acceleration -= Up;        // Q = down (-Y)
    
            if (glm::length(Acceleration) > 0.0f)
                Acceleration = glm::normalize(Acceleration) * Speed;
    
            // Integrate acceleration to velocity
            Velocity.Velocity += Acceleration * (float)DeltaTime;
    
            // Apply simple linear drag
            constexpr float Drag = 10.0f;
            Velocity.Velocity -= Velocity.Velocity * Drag * (float)DeltaTime;
    
            // Apply velocity to position
            Transform.Transform.Location += Velocity.Velocity * (float)DeltaTime;
    
            // Mouse look
            if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
            {
                GEngine->GetEngineSubsystem<FInputSubsystem>()->SetCursorMode(GLFW_CURSOR_DISABLED);
    
                float MousePitchDelta = Input::GetMouseDeltaPitch();
                float MouseYawDelta   = Input::GetMouseDeltaYaw();
    
                constexpr float Sensitivity = 0.1f;
                float YawDelta   = -MouseYawDelta   * Sensitivity;
                float PitchDelta = -MousePitchDelta * Sensitivity;
    
                glm::quat yawQuat = glm::angleAxis(glm::radians(YawDelta), FViewVolume::UpAxis);
                glm::vec3 RightAxis = Transform.Transform.Rotation * FViewVolume::RightAxis;
                glm::quat pitchQuat = glm::angleAxis(glm::radians(PitchDelta), RightAxis);
    
                Transform.Transform.Rotation = glm::normalize(yawQuat * pitchQuat * Transform.Transform.Rotation);
            }
            else
            {
                GEngine->GetEngineSubsystem<FInputSubsystem>()->SetCursorMode(GLFW_CURSOR_NORMAL);
            }
    
            // Update camera view
            glm::vec3 UpdatedForward = Transform.Transform.Rotation * FViewVolume::ForwardAxis;
            glm::vec3 UpdatedUp = Transform.Transform.Rotation * FViewVolume::UpAxis;
    
            Camera.SetView(Transform.Transform.Location, UpdatedForward, UpdatedUp);
        }
    }
}
