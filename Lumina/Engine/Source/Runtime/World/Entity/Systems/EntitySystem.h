#pragma once
#include "Core/UpdateStage.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "SystemContext.h"
#include "EntitySystem.generated.h"


namespace Lumina
{
    class FEntityRegistry;
    class Entity;
    class FSubsystemManager;
}

namespace Lumina
{

    LUM_CLASS()
    class LUMINA_API CEntitySystemRegistry : public CObject
    {
        GENERATED_BODY()
    public:

        void RegisterSystem(class CEntitySystem* NewSystem);

        static CEntitySystemRegistry& Get();

        void GetRegisteredSystems(TVector<TObjectHandle<CEntitySystem>>& Systems);

        TVector<TObjectHandle<class CEntitySystem>> RegisteredSystems;

        static CEntitySystemRegistry* Singleton;

    };
    

    LUM_CLASS()
    class LUMINA_API CEntitySystem : public CObject
    {
        GENERATED_BODY()
        
    public:

        friend class CWorld;
        
        void PostCreateCDO() override;
        
        /** Retrieves the update priority and stage for this system */
        virtual const FUpdatePriorityList* GetRequiredUpdatePriorities() { return nullptr; }

        /** Called when the system is first constructed for the world during play */
        virtual void Initialize(FSystemContext& SystemContext) { }

        /** Called when the system is first constructed for the world in editor */
        virtual void InitializeEditor(FSystemContext& SystemContext) { }

        /** Called per-update, for each required system */
        virtual void Update(FSystemContext& SystemContext) { }

        /** Called when the system is removed from the world, (and world shutdown) */
        virtual void Shutdown(FSystemContext& SystemContext) { }
        
    };
}


#define ENTITY_SYSTEM(Type, ... )\
static const FUpdatePriorityList PriorityList; \
virtual const FUpdatePriorityList* GetRequiredUpdatePriorities() override { static const FUpdatePriorityList PriorityList = FUpdatePriorityList(__VA_ARGS__); return &PriorityList; }
