#pragma once

#include "Module/API.h"
#include "RenderComponent.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "MeshComponent.generated.h"
#include "World/Scene/RenderScene/MeshBatch.h"

namespace Lumina
{
    class CMaterialInterface;
    
    LUM_STRUCT()
    struct LUMINA_API SMeshComponent : SRenderComponent
    {
        GENERATED_BODY()
        
        LUM_PROPERTY(Editable, Category = "Materials")
        TVector<TObjectPtr<CMaterialInterface>> MaterialOverrides;

    };
    
}
