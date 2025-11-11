#include "Engine.h"
#include "Assets/AssetManager/AssetManager.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Core/Application/Application.h"
#include "Core/Delegates/CoreDelegates.h"
#include "Core/Module/ModuleManager.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/Package/Package.h"
#include "Core/Profiler/Profile.h"
#include "Core/Windows/Window.h"
#include "Input/InputSubsystem.h"
#include "Physics/Physics.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderManager.h"
#include "Renderer/RenderResource.h"
#include "Renderer/RHIGlobals.h"
#include "TaskSystem/TaskSystem.h"
#include "Tools/UI/DevelopmentToolUI.h"
#include "World/WorldManager.h"

namespace Lumina
{
    LUMINA_API FEngine* GEngine;
    
    bool FEngine::Init(FApplication* App)
    {
        //-------------------------------------------------------------------------
        // Initialize core engine state.
        //-------------------------------------------------------------------------
        
        LUMINA_PROFILE_SCOPE();
        FCoreDelegates::OnPreEngineInit.BroadcastAndClear();

        
        GEngine = this;
        Application = App;
        
        FTaskSystem::Initialize();
        Physics::Initialize();
        
        RenderManager = EngineSubsystems.AddSubsystem<FRenderManager>();
        EngineViewport = GRenderContext->CreateViewport(Windowing::GetPrimaryWindowHandle()->GetExtent());

        ProcessNewlyLoadedCObjects();
        
        FEntityComponentRegistry::Get().RegisterAll();
        
        InputSubsystem = EngineSubsystems.AddSubsystem<FInputSubsystem>();
        AssetRegistry = EngineSubsystems.AddSubsystem<FAssetRegistry>();
        AssetManager = EngineSubsystems.AddSubsystem<FAssetManager>();
        WorldManager = EngineSubsystems.AddSubsystem<FWorldManager>();
        
        UpdateContext.SubsystemManager = &EngineSubsystems;

        #if WITH_DEVELOPMENT_TOOLS
        DeveloperToolUI = CreateDevelopmentTools();
        DeveloperToolUI->Initialize(UpdateContext);
        #endif
        
        FCoreDelegates::OnPostEngineInit.BroadcastAndClear();
        
        return true;
    }

    bool FEngine::Shutdown()
    {
        LUMINA_PROFILE_SCOPE();

        FCoreDelegates::OnPreEngineShutdown.BroadcastAndClear();

        //-------------------------------------------------------------------------
        // Shutdown core engine state.
        //-------------------------------------------------------------------------


        #if WITH_DEVELOPMENT_TOOLS
        DeveloperToolUI->Deinitialize(UpdateContext);
        delete DeveloperToolUI;
        #endif

        EngineSubsystems.RemoveSubsystem<FWorldManager>();
        EngineSubsystems.RemoveSubsystem<FAssetManager>();
        EngineSubsystems.RemoveSubsystem<FAssetRegistry>();
        EngineSubsystems.RemoveSubsystem<FInputSubsystem>();
        
        ShutdownCObjectSystem();
        
        EngineViewport.SafeRelease();
        
        EngineSubsystems.RemoveSubsystem<FRenderManager>();

        Physics::Shutdown();
        
        FTaskSystem::Shutdown();
        
        FModuleManager::Get().UnloadAllModules();
        
        return false;
    }

    bool FEngine::Update(bool bApplicationWantsExit)
    {
        LUMINA_PROFILE_SCOPE();

        //-------------------------------------------------------------------------
        // Update core engine state.
        //-------------------------------------------------------------------------

        bEngineReadyToClose = true;
        bCloseRequested = bApplicationWantsExit;
        
        if (!Windowing::GetPrimaryWindowHandle()->IsMinimized())
        {
            // Frame Start
            //-------------------------------------------------------------------
            {
                LUMINA_PROFILE_SECTION_COLORED("FrameStart", tracy::Color::Red);
                UpdateContext.UpdateStage = EUpdateStage::FrameStart;
                
                RenderManager->FrameStart(UpdateContext);
                
                InputSubsystem->Update(UpdateContext);
                
                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->StartFrame(UpdateContext);
                DeveloperToolUI->Update(UpdateContext);
                #endif
                
                WorldManager->UpdateWorlds(UpdateContext);
                
                OnUpdateStage(UpdateContext);
            }

            // Pre Physics
            //-------------------------------------------------------------------
            {
                LUMINA_PROFILE_SECTION_COLORED("Pre-Physics", tracy::Color::Green);
                UpdateContext.UpdateStage = EUpdateStage::PrePhysics;


                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->Update(UpdateContext);
                #endif
                
                WorldManager->UpdateWorlds(UpdateContext);

                OnUpdateStage(UpdateContext);
            }

            // During Physics
            //-------------------------------------------------------------------
            {
                LUMINA_PROFILE_SECTION_COLORED("During-Physics", tracy::Color::Blue);
                UpdateContext.UpdateStage = EUpdateStage::DuringPhysics;


                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->Update(UpdateContext);
                #endif
                
                WorldManager->UpdateWorlds(UpdateContext);

                OnUpdateStage(UpdateContext);
            }

            // Post Physics
            //-------------------------------------------------------------------
            {
                LUMINA_PROFILE_SECTION_COLORED("Post-Physics", tracy::Color::Yellow);
                UpdateContext.UpdateStage = EUpdateStage::PostPhysics;


                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->Update(UpdateContext);
                #endif

                WorldManager->UpdateWorlds(UpdateContext);

                OnUpdateStage(UpdateContext);
            }

            // Paused
            //-------------------------------------------------------------------
            {
                LUMINA_PROFILE_SECTION_COLORED("Paused", tracy::Color::Purple);
                UpdateContext.UpdateStage = EUpdateStage::Paused;

                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->Update(UpdateContext);
                #endif

                WorldManager->UpdateWorlds(UpdateContext);

                OnUpdateStage(UpdateContext);
            }

            // Frame End / Render
            //-------------------------------------------------------------------
            {
                FRenderGraph RenderGraph;

                LUMINA_PROFILE_SECTION_COLORED("Frame-End", tracy::Color::Coral);
                UpdateContext.UpdateStage = EUpdateStage::FrameEnd;

                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->Update(UpdateContext);
                #endif

                WorldManager->UpdateWorlds(UpdateContext);
                WorldManager->RenderWorlds(RenderGraph);
                
                #if WITH_DEVELOPMENT_TOOLS
                DeveloperToolUI->EndFrame(UpdateContext);
                #endif
                
                RenderManager->FrameEnd(UpdateContext, RenderGraph);

                OnUpdateStage(UpdateContext);

            }
        }

        UpdateContext.MarkFrameEnd(glfwGetTime());

        if (bApplicationWantsExit)
        {
            return !bEngineReadyToClose;
        }
        
        return true;
    }

    #if WITH_DEVELOPMENT_TOOLS
    void FEngine::DrawDevelopmentTools()
    {
        if (Application->HasAnyFlags(EApplicationFlags::DevelopmentTools))
        {
            Application->RenderDeveloperTools(UpdateContext);
        }
    }
    #endif

    entt::meta_ctx& FEngine::GetEngineMetaContext() const
    {
        return entt::locator<entt::meta_ctx>::value_or();
    }

    entt::locator<entt::meta_ctx>::node_type FEngine::GetEngineMetaService() const
    {
        return entt::locator<entt::meta_ctx>::handle();
    }
    
    void FEngine::SetEngineViewportSize(const glm::uvec2& InSize)
    {
        EngineViewport->SetSize(InSize);
    }

}
