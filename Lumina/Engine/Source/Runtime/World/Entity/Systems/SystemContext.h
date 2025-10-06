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
        
        FSystemContext(CWorld* World);
        ~FSystemContext();
        

        double GetDeltaTime() const { return DeltaTime; }
        
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
        
    private:
        
        double DeltaTime = 0.0f;
        FEntityWorld* EntityWorld = nullptr;
    };
}
