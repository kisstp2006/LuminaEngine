#include "SystemContext.h"

#include "World/World.h"

namespace Lumina
{
    FSystemContext::FSystemContext(CWorld* InWorld)
    {
        World = InWorld;
        EntityWorld = &const_cast<CWorld*>(World)->EntityWorld;
        EntityWorld->Lock();
        DeltaTime = World->GetWorldDeltaTime();
    }

    FSystemContext::~FSystemContext()
    {
        EntityWorld->Unlock();
    }

    void FSystemContext::DrawDebugLine(const glm::vec3& Start, const glm::vec3& End, const glm::vec4& Color, float Thickness, float Duration) const
    {
        World->DrawDebugLine(Start, End, Color, Thickness, Duration);
    }

    void FSystemContext::DrawDebugBox(const glm::vec3& Center, const glm::vec3& Extents, const glm::quat& Rotation, const glm::vec4& Color, float Thickness, float Duration) const
    {
        World->DrawDebugBox(Center, Extents, Rotation, Color, Thickness, Duration);
    }

    void FSystemContext::DrawDebugSphere(const glm::vec3& Center, float Radius, const glm::vec4& Color, uint8 Segments, float Thickness, float Duration) const
    {
        World->DrawDebugSphere(Center, Radius, Color, Segments, Thickness, Duration);
    }

    void FSystemContext::DrawDebugCone(const glm::vec3& Apex, const glm::vec3& Direction, float AngleRadians, float Length, const glm::vec4& Color, uint8 Segments, uint8 Stacks, float Thickness, float Duration) const
    {
        World->DrawDebugCone(Apex, Direction, AngleRadians, Length, Color, Segments, Stacks, Thickness, Duration);
    }

    void FSystemContext::DrawFrustum(const glm::mat4& Matrix, float zNear, float zFar, const glm::vec4& Color, float Thickness, float Duration) const
    {
        World->DrawFrustum(Matrix, zNear, zFar, Color, Thickness, Duration);
    }

    void FSystemContext::DrawArrow(const glm::vec3& Start, const glm::vec3& Direction, float Length, const glm::vec4& Color, float Thickness, float Duration, float HeadSize) const
    {
        World->DrawArrow(Start, Direction, Length, Color, Thickness, Duration, HeadSize);
    }

    void FSystemContext::DrawViewVolume(const FViewVolume& ViewVolume, const glm::vec4& Color, float Thickness, float Duration) const
    {
        World->DrawViewVolume(ViewVolume, Color, Thickness, Duration);
    }
}
