#pragma once
#include "Components/Component.h"
#include "Core/Math/Transform.h"
#include "Registry/EntityRegistry.h"

namespace Lumina
{
    /**
     * An entity world is the representation of all entities in a world.
     */
    class FEntityWorld
    {
        friend class CWorld;
    public:

        void Lock() { bReadOnly.store(true); }
        void Unlock() { bReadOnly.store(false); }
        bool IsReadOnly() const { return bReadOnly.load(std::memory_order_relaxed); }

        template<typename... Ts, typename... TArgs>
        auto CreateView(TArgs&&... Args)
        {
            return Registry.view<Ts...>(std::forward<TArgs>(Args)...);
        }

        template<typename... Ts, typename... TArgs>
        auto CreateGroup(TArgs&&... Args)
        {
            return Registry.group<Ts...>(std::forward<TArgs>(Args)...);
        }

        template<typename T, typename ... TArgs>
        T& Emplace(entt::entity entity, TArgs&& ... Args)
        {
            return Registry.emplace<T>(entity, std::forward<TArgs>(Args)...);
        }

        template<typename T, typename ... TArgs>
        T& EmplaceOrReplace(entt::entity entity, TArgs&& ... Args)
        {
            return Registry.emplace_or_replace<T>(entity, std::forward<TArgs>(Args)...);
        }

        LUMINA_API entt::entity Create(const FName& Name, const FTransform& Transform = FTransform());
        LUMINA_API entt::entity Create();

        template<typename T>
        void Erase(entt::entity Entt)
        {
            Registry.erase<T>(Entt);
        }

        template<typename... Ts>
        void Clear()
        {
            Registry.clear<Ts...>();
        }

        template<typename... Ts>
        auto TryGet(entt::entity entity)
        {
            return Registry.try_get<Ts...>(entity);
        }

        template<typename... Ts>
        decltype(auto) Get(entt::entity entity)
        {
            return Registry.get<Ts...>(entity);
        }

        template<bool bAnyOf, typename... Ts>
        bool Has(entt::entity entity) const
        {
            if constexpr (bAnyOf)
            {
                return Registry.any_of<Ts...>(entity);
            }
            else
            {
                return Registry.all_of<Ts...>(entity);
            }
        }
    
        
    
    private:

        std::atomic_bool bReadOnly{false};
        FEntityRegistry Registry;
    };
}
