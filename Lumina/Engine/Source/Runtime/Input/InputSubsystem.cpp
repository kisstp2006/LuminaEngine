
#include "InputSubsystem.h"
#include "Core/Profiler/Profile.h"
#include "Core/Windows/Window.h"

namespace Lumina
{
    void FInputSubsystem::Initialize()
    {
    }

    void FInputSubsystem::Update(const FUpdateContext& UpdateContext)
    {
        LUMINA_PROFILE_SCOPE();

        FWindow* PrimaryWindow = Windowing::GetPrimaryWindowHandle();
        GLFWwindow* Window = PrimaryWindow->GetWindow();

        int Mode = glfwGetInputMode(Window, GLFW_CURSOR);
        int DesiredMode = Snapshot.CursorMode;
        if (Mode != DesiredMode)
        {
            glfwSetInputMode(Window, GLFW_CURSOR, DesiredMode);
        }


        double xpos, ypos;
        glfwGetCursorPos(Window, &xpos, &ypos);
        glm::vec2 MousePos = {xpos, ypos};
        if (MousePosLastFrame == glm::vec2(0.0f))
        {
            MousePosLastFrame = MousePos;
            return;
        }
        
        MouseDeltaPitch = MousePos.y - MousePosLastFrame.y;
        MouseDeltaYaw = MousePos.x - MousePosLastFrame.x;
        

        // Keys
        for (int k = Key::Space; k < Key::Num; ++k)
        {
            Snapshot.Keys[k] = glfwGetKey(Window, k) == GLFW_PRESS;
        }

        // Mouse
        for (int b = 0; b < Mouse::Num; ++b)
        {
            Snapshot.MouseButtons[b] = glfwGetMouseButton(Window, b) == GLFW_PRESS;
        }

        // Cursor
        Snapshot.MouseX = static_cast<float>(xpos);
        Snapshot.MouseY = static_cast<float>(ypos);
        
        MousePosLastFrame = MousePos;
    }

    void FInputSubsystem::Deinitialize()
    {
    }
    

    void FInputSubsystem::SetCursorMode(int NewMode)
    {
        Snapshot.CursorMode = NewMode;;
    }

    bool FInputSubsystem::IsKeyPressed(KeyCode Key)
    {
        return Snapshot.Keys[Key];
    }

    bool FInputSubsystem::IsMouseButtonPressed(MouseCode Button)
    {
        return Snapshot.MouseButtons[Button];
    }

    glm::vec2 FInputSubsystem::GetMousePosition() const
    {
        return glm::vec2(Snapshot.MouseX, Snapshot.MouseY);
    }

}

