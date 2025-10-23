#pragma once
#include "Core/Object/ObjectMacros.h"
#include "EntitySystem.h"
#include "UpdateTransformEntitySystem.generated.h"

namespace Lumina
{
    LUM_CLASS()
    class LUMINA_API CUpdateTransformEntitySystem : public CEntitySystem
    {
        GENERATED_BODY()
        ENTITY_SYSTEM(CUpdateTransformEntitySystem, RequiresUpdate(EUpdateStage::PrePhysics), RequiresUpdate(EUpdateStage::Paused))
    public:

        
        void Initialize(FSystemContext& SystemContext) override;
        void Shutdown(FSystemContext& SystemContext) override;

        void Update(FSystemContext& SystemContext) override;
        
    
    };
}
