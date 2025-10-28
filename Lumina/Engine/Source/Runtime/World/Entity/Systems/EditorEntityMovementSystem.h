#pragma once
#include "EntitySystem.h"
#include "glm/glm.hpp"
#include "EditorEntityMovementSystem.generated.h"


namespace Lumina
{
    LUM_CLASS()
    class LUMINA_API CEditorEntityMovementSystem : public CEntitySystem
    {
        GENERATED_BODY()
        ENTITY_SYSTEM(CEditorEntityMovementSystem, RequiresUpdate(EUpdateStage::Paused))
    public:
        
        void Initialize(FSystemContext& SystemContext) override;
        void Shutdown(FSystemContext& SystemContext) override;

        void Update(FSystemContext& SystemContext) override;

    private:

    };
}
