#include "ViewVolume.h"
#include "Renderer/RHIIncl.h"

namespace Lumina
{

    glm::vec3 FViewVolume::UpAxis        = glm::vec3(0.0f,  1.0f,  0.0f);
    glm::vec3 FViewVolume::DownAxis      = glm::vec3(0.0f, -1.0f,  0.0f);
    glm::vec3 FViewVolume::RightAxis     = glm::vec3(1.0f,  0.0f,  0.0f);
    glm::vec3 FViewVolume::LeftAxis      = glm::vec3(-1.0f, 0.0f,  0.0f);
    glm::vec3 FViewVolume::ForwardAxis   = glm::vec3(0.0f,  0.0f,  1.0f);
    glm::vec3 FViewVolume::BackwardAxis  = glm::vec3(0.0f,  0.0f, -1.0f);
    
    FViewVolume::FViewVolume(float fov, float aspect, float InNear, float InFar)
        : ViewPosition(glm::vec3(1.0))
        , Near(InNear)
        , Far(InFar)
        , FOV(fov)
        , AspectRatio(aspect)
    {
        RightVector         = glm::normalize(glm::cross(UpAxis, ForwardAxis));
        UpVector            = glm::normalize(glm::cross(RightAxis, ForwardVector));

        SetPerspective(fov, aspect);
    }

    FViewVolume& FViewVolume::SetNear(float InNear)
    {
        Near = InNear;
        ProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, Far, Near);
        UpdateMatrices();

        return *this;
    }

    FViewVolume& FViewVolume::SetFar(float InFar)
    {
        Far = InFar;
        ProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, Far, Near);
        UpdateMatrices();

        return *this;
    }


    FViewVolume& FViewVolume::SetViewPosition(const glm::vec3& Position)
    {
        ViewPosition = Position;
        UpdateMatrices();

        return *this;
    }
    
    FViewVolume& FViewVolume::SetView(const glm::vec3& Position, const glm::vec3& ViewDirection, const glm::vec3& UpDirection)
    {
        ViewPosition = Position;

        UpVector        = glm::normalize(UpDirection);
        ForwardVector   = glm::normalize(ViewDirection);
        RightVector     = glm::normalize(glm::cross(UpVector, ForwardVector));
        UpVector        = glm::normalize(glm::cross(RightVector, ForwardVector));

        UpdateMatrices();

        return *this;
    }


    FViewVolume& FViewVolume::SetPerspective(float fov, float aspect)
    {
        FOV = fov;
        AspectRatio = aspect;

        ProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, Far, Near);
        UpdateMatrices();

        return *this;
    }


    FViewVolume& FViewVolume::SetAspectRatio(float InAspect)
    {
        AspectRatio = InAspect;

        ProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, Far, Near);
        UpdateMatrices();

        return *this;
    }
    
    FViewVolume& FViewVolume::SetFOV(float InFOV)
    {
        FOV = InFOV;
        ProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, Far, Near);
        UpdateMatrices();

        return *this;
    }

    FViewVolume& FViewVolume::Rotate(float Angle, glm::vec3 Axis)
    {
        ViewMatrix = glm::rotate(ViewMatrix, Angle, Axis);
        UpdateMatrices();

        return *this;
    }

    glm::mat4 FViewVolume::ToReverseDepthViewProjectionMatrix() const
    {
        glm::mat4 ReverseProjectionMatrix = glm::perspective(glm::radians(FOV), AspectRatio, Near, Far);
        return ReverseProjectionMatrix * ViewMatrix;
    }

    FFrustum FViewVolume::GetFrustum() const
    {
        const glm::mat4& matrix = ViewProjectionMatrix;

        FFrustum Frustum = {};
        Frustum.Planes[FFrustum::LEFT].x = matrix[0].w + matrix[0].x;
        Frustum.Planes[FFrustum::LEFT].y = matrix[1].w + matrix[1].x;
        Frustum.Planes[FFrustum::LEFT].z = matrix[2].w + matrix[2].x;
        Frustum.Planes[FFrustum::LEFT].w = matrix[3].w + matrix[3].x;

        Frustum.Planes[FFrustum::RIGHT].x = matrix[0].w - matrix[0].x;
        Frustum.Planes[FFrustum::RIGHT].y = matrix[1].w - matrix[1].x;
        Frustum.Planes[FFrustum::RIGHT].z = matrix[2].w - matrix[2].x;
        Frustum.Planes[FFrustum::RIGHT].w = matrix[3].w - matrix[3].x;

        Frustum.Planes[FFrustum::TOP].x = matrix[0].w - matrix[0].y;
        Frustum.Planes[FFrustum::TOP].y = matrix[1].w - matrix[1].y;
        Frustum.Planes[FFrustum::TOP].z = matrix[2].w - matrix[2].y;
        Frustum.Planes[FFrustum::TOP].w = matrix[3].w - matrix[3].y;

        Frustum.Planes[FFrustum::BOTTOM].x = matrix[0].w + matrix[0].y;
        Frustum.Planes[FFrustum::BOTTOM].y = matrix[1].w + matrix[1].y;
        Frustum.Planes[FFrustum::BOTTOM].z = matrix[2].w + matrix[2].y;
        Frustum.Planes[FFrustum::BOTTOM].w = matrix[3].w + matrix[3].y;

        Frustum.Planes[FFrustum::BACK].x = matrix[0].w + matrix[0].z;
        Frustum.Planes[FFrustum::BACK].y = matrix[1].w + matrix[1].z;
        Frustum.Planes[FFrustum::BACK].z = matrix[2].w + matrix[2].z;
        Frustum.Planes[FFrustum::BACK].w = matrix[3].w + matrix[3].z;

        Frustum.Planes[FFrustum::FRONT].x = matrix[0].w - matrix[0].z;
        Frustum.Planes[FFrustum::FRONT].y = matrix[1].w - matrix[1].z;
        Frustum.Planes[FFrustum::FRONT].z = matrix[2].w - matrix[2].z;
        Frustum.Planes[FFrustum::FRONT].w = matrix[3].w - matrix[3].z;

        for (auto i = 0; i < FFrustum::NUM; i++)
        {
            float length = glm::sqrt(Frustum.Planes[i].x * Frustum.Planes[i].x + Frustum.Planes[i].y * Frustum.Planes[i].y + Frustum.Planes[i].z * Frustum.Planes[i].z);
            Frustum.Planes[i] /= length;
        }

        return Frustum;
    }
    
    void FViewVolume::UpdateMatrices()
    {
        ViewMatrix = glm::lookAt(ViewPosition, ViewPosition + ForwardVector, UpVector);
        ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
    }
}
