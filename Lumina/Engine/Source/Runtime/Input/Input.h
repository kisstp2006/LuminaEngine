#pragma once
#include "InputSubsystem.h"
#include "Events/KeyCodes.h"

namespace Lumina
{
    namespace Input
    {
        LUMINA_API inline bool IsKeyPressed(KeyCode Key)
        {
            return GEngine->GetEngineSubsystem<FInputSubsystem>()->IsKeyPressed(Key);
        }

        LUMINA_API inline bool IsMouseButtonPressed(MouseCode Button)
        {
            return GEngine->GetEngineSubsystem<FInputSubsystem>()->IsMouseButtonPressed(Button);
        }

        LUMINA_API inline glm::vec2 GetMousePosition()
        {
            return GEngine->GetEngineSubsystem<FInputSubsystem>()->GetMousePosition();
        }

        LUMINA_API inline float GetMouseDeltaPitch()
        {
            return GEngine->GetEngineSubsystem<FInputSubsystem>()->GetMouseDeltaPitch();
        }

        LUMINA_API inline float GetMouseDeltaYaw()
        {
            return GEngine->GetEngineSubsystem<FInputSubsystem>()->GetMouseDeltaYaw();
        }
    }
}
