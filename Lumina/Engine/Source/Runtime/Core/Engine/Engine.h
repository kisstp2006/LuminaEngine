#pragma once

#include "Module/API.h"
#include "Lumina.h"
#include "Core/UpdateContext.h"
#include "Core/Delegates/Delegate.h"
#include "entt/entt.hpp"
#include "Renderer/RHIFwd.h"
#include "Renderer/RHIIncl.h"
#include "Subsystems/Subsystem.h"


namespace Lumina
{
    class FWorldManager;
    class FAssetRegistry;
    class FRenderManager;
    class IImGuiRenderer;
    class IDevelopmentToolUI;
    class FAssetManager;
    class FInputSubsystem;
    class FApplication;
    class FWindow;
    class FRenderScene;
}

namespace Lumina
{
    class FEngine
    {
    public:
        
        FEngine() = default;
        virtual ~FEngine() = default;

        LUMINA_API virtual bool Init(FApplication* App);
        LUMINA_API virtual bool Shutdown();
        LUMINA_API bool Update();
        LUMINA_API virtual void OnUpdateStage(const FUpdateContext& Context) { }; 

        LUMINA_API const FRHIViewportRef& GetEngineViewport() const { return EngineViewport; }
        
        LUMINA_API void SetEngineViewportSize(const FIntVector2D& InSize);

        #if WITH_DEVELOPMENT_TOOLS
        LUMINA_API virtual IDevelopmentToolUI* CreateDevelopmentTools() = 0;
        LUMINA_API virtual void DrawDevelopmentTools();
        LUMINA_API IDevelopmentToolUI* GetDevelopmentToolsUI() const { return DeveloperToolUI; }
        #endif

        LUMINA_API entt::meta_ctx& GetEngineMetaContext() const;
        LUMINA_API entt::locator<entt::meta_ctx>::node_type GetEngineMetaService() const;

        
        template<typename T>
        T* GetEngineSubsystem()
        {
            return EngineSubsystems.GetSubsystem<T>();
        }

        FORCEINLINE const FUpdateContext& GetUpdateContext() const { return UpdateContext; }
    
    protected:
        
        FUpdateContext          UpdateContext;
        FApplication*           Application =           nullptr;

        #if WITH_DEVELOPMENT_TOOLS
        IDevelopmentToolUI*     DeveloperToolUI =       nullptr;
        #endif

        FSubsystemManager       EngineSubsystems;
        FInputSubsystem*        InputSubsystem =        nullptr;
        FAssetManager*          AssetManager =          nullptr;
        FAssetRegistry*         AssetRegistry =         nullptr;
        FWorldManager*          WorldManager =          nullptr;
        FRenderManager*         RenderManager =         nullptr;
        
        
        FRHIViewportRef         EngineViewport;
    };
    
    LUMINA_API extern FEngine* GEngine;
    
    template<typename T>
    static T& GetEngineSystem()
    {
        return *GEngine->GetEngineSubsystem<T>();
    }
    
}
