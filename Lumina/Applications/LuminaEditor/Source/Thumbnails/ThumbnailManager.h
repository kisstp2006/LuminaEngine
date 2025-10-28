#pragma once
#include "Core/Object/ObjectMacros.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "ThumbnailManager.generated.h"
#include "Memory/SmartPtr.h"
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    struct FPackageThumbnail;
    class CStaticMesh;
}


namespace Lumina
{
    LUM_CLASS()
    class CThumbnailManager : public CObject
    {
        GENERATED_BODY()
    public:

        CThumbnailManager();

        void Initialize();
        
        static CThumbnailManager& Get();

        void GetOrLoadThumbnailsForPackage(const FString& PackagePath);


        LUM_PROPERTY(NotSerialized)
        TObjectHandle<CStaticMesh> CubeMesh;

        LUM_PROPERTY(NotSerialized)
        TObjectHandle<CStaticMesh> SphereMesh;

        LUM_PROPERTY(NotSerialized)
        TObjectHandle<CStaticMesh> PlaneMesh;
        
        static CThumbnailManager* ThumbnailManagerSingleton;
        
    };
}
