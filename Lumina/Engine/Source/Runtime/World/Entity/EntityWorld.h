#pragma once
#include "Components/Component.h"
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

        template<typename T, typename ... TArgs>
        T& Emplace(TArgs&& ... Args)
        {
            return Registry.emplace<T>(std::forward<TArgs>(Args)...);
        }

        template<typename T>
        void Erase(entt::entity Entt)
        {
            Registry.erase<T>(Entt);
        }
    
    private:

        std::atomic_bool bReadOnly{false};
        FEntityRegistry Registry;
    };
}
