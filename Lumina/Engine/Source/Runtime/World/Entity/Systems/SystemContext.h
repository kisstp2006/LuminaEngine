#pragma once
#include "World/Entity/EntityWorld.h"

namespace Lumina
{
    class CWorld;
    class FEntityWorld;
}

namespace Lumina
{
    /** Encapsulates the data a world system can execute, this may seem redundant, but it's more-so to control
     * what systems can access.
     * 
     */
    struct FSystemContext
    {
        friend class CWorld;
        
        FSystemContext(CWorld* InWorld);
        ~FSystemContext();
        

        LUMINA_API double GetDeltaTime() const { return DeltaTime; }
        
        template<typename... Ts, typename... TArgs>
        auto CreateView(TArgs&&... Args) -> decltype(std::declval<entt::registry>().view<Ts...>(std::forward<TArgs>(Args)...))
        {
            return EntityWorld->CreateView<Ts...>(std::forward<TArgs>(Args)...);
        }

        template<typename... Ts, typename ... TArgs>
        auto CreateGroup(TArgs&&... Args)
        {
            return EntityWorld->CreateGroup<Ts...>(std::forward<TArgs>(Args)...);
        }

        template<typename... Ts>
        decltype(auto) Get(entt::entity entity)
        {
            return EntityWorld->Get<Ts...>(entity);
        }
        
        template<typename... Ts>
        void Clear() const
        {
            EntityWorld->Clear<Ts...>();
        }

        template<bool bAnyOf, typename... Ts>
        bool Has(entt::entity entity) const
        {
            return EntityWorld->Has<bAnyOf, Ts...>(entity);
        }

        template<typename T, typename ... TArgs>
        T& Emplace(TArgs&& ... Args)
        {
            return EntityWorld->Emplace<T>(std::forward<TArgs>(Args)...);
        }

        template<typename T, typename ... TArgs>
        T& EmplaceOrReplace(entt::entity entity, TArgs&& ... Args)
        {
            return EntityWorld->EmplaceOrReplace<T>(entity, std::forward<TArgs>(Args)...);
        }

        LUMINA_API entt::entity Create(const FName& Name, const FTransform& Transform = FTransform()) const
        {
            return EntityWorld->Create(Name, Transform);
        }

        LUMINA_API entt::entity Create() const
        {
            return EntityWorld->Create();
        }

        LUMINA_API const CWorld* GetWorld() const
        {
            return World;
        }

        LUMINA_API void DrawDebugLine(const glm::vec3& Start, const glm::vec3& End, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f) const;
        LUMINA_API void DrawDebugBox(const glm::vec3& Center, const glm::vec3& Extents, const glm::quat& Rotation, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f) const;
        LUMINA_API void DrawDebugSphere(const glm::vec3& Center, float Radius, const glm::vec4& Color, uint8 Segments = 16, float Thickness = 1.0f, float Duration = 1.0f) const;
        LUMINA_API void DrawDebugCone(const glm::vec3& Apex, const glm::vec3& Direction, float AngleRadians, float Length, const glm::vec4& Color, uint8 Segments = 16, uint8 Stacks = 4, float Thickness = 1.0f, float Duration = 1.0f) const;
        LUMINA_API void DrawFrustum(const glm::mat4& Matrix, float zNear, float zFar, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f) const;
        LUMINA_API void DrawArrow(const glm::vec3& Start, const glm::vec3& Direction, float Length, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f, float HeadSize = 0.2f) const;
        LUMINA_API void DrawViewVolume(const FViewVolume& ViewVolume, const glm::vec4& Color, float Thickness = 1.0f, float Duration = 1.0f) const;
        
    private:
        
        double DeltaTime = 0.0f;
        CWorld* World = nullptr;
        FEntityWorld* EntityWorld = nullptr;
    };
}
