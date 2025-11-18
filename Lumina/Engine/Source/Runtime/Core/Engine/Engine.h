#pragma once

#include "Lumina.h"
#include "Core/UpdateContext.h"
#include "entt/entt.hpp"
#include "Module/API.h"
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
    class FDeferredRenderScene;
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
        LUMINA_API bool Update(bool bApplicationWantsExit);
        LUMINA_API virtual void OnUpdateStage(const FUpdateContext& Context) { }

        LUMINA_API FRHIViewport* GetEngineViewport() const { return EngineViewport; }
        
        LUMINA_API void SetEngineViewportSize(const glm::uvec2& InSize);

        #if WITH_DEVELOPMENT_TOOLS
        LUMINA_API virtual IDevelopmentToolUI* CreateDevelopmentTools() = 0;
        LUMINA_API virtual void DrawDevelopmentTools();
        LUMINA_API IDevelopmentToolUI* GetDevelopmentToolsUI() const { return DeveloperToolUI; }
        #endif

        LUMINA_API entt::meta_ctx& GetEngineMetaContext() const;
        LUMINA_API entt::locator<entt::meta_ctx>::node_type GetEngineMetaService() const;

        LUMINA_API void SetReadyToClose(bool bReadyToClose) { bEngineReadyToClose = bReadyToClose; }
        
        template<typename T>
        T* GetEngineSubsystem()
        {
            return EngineSubsystems.GetSubsystem<T>();
        }

        FORCEINLINE const FUpdateContext& GetUpdateContext() const { return UpdateContext; }

        LUMINA_API void SetEngineReadyToClose(bool bReady) { bEngineReadyToClose = bReady; }
        LUMINA_API FORCEINLINE bool IsCloseRequested() const { return bCloseRequested; }
        
    private:
    
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


        bool                    bCloseRequested = false;
        bool                    bEngineReadyToClose = false;
    };
    
    LUMINA_API extern FEngine* GEngine;
    
    template<typename T>
    static T& GetEngineSystem()
    {
        return *GEngine->GetEngineSubsystem<T>();
    }
    
}
