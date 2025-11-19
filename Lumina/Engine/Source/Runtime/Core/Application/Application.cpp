
#include "Application.h"
#include "ApplicationGlobalState.h"
#include "Assets/AssetManager/AssetManager.h"
#include "Core/Module/ModuleManager.h"
#include "Core/Utils/CommandLineParser.h"
#include "Core/Windows/Window.h"
#include "Core/Windows/WindowTypes.h"
#include "Paths/Paths.h"
#include "Platform/Process/PlatformProcess.h"
#include "Project/Project.h"
#include "tracy/Tracy.hpp"

namespace Lumina
{
    FApplication* FApplication::Instance = nullptr;
    FCommandLineParser FApplication::CommandLine;

    FApplication::FApplication(const FString& InApplicationName, uint32 AppFlags)
    {
        ApplicationName = InApplicationName;
        ApplicationFlags = AppFlags;
        Instance = this;
    }

    int32 FApplication::Run(int argc, char** argv)
    {
        LUMINA_PROFILE_SCOPE();
        CommandLine.Parse(argc, argv);
        
        LOG_TRACE("Initializing Application: {0}", ApplicationName);
        
        //---------------------------------------------------------------
        // Application initialization.
        //--------------------------------------------------------------

        PreInitStartup();
        CreateApplicationWindow();
        CreateEngine();

        
        if (!Initialize(argc, argv))
        {
            Shutdown();
            return 1;
        }
        
        //---------------------------------------------------------------
        // Core application loop.
        //--------------------------------------------------------------

        bool bEngineWantsExit = false;
        while(!bEngineWantsExit)
        {
            LUMINA_PROFILE_FRAME();

            MainWindow->ProcessMessages();

            bool bApplicationWantsExit = ShouldExit();
            
            bEngineWantsExit = !GEngine->Update(bApplicationWantsExit);
        }

        
        //---------------------------------------------------------------
        // Application shutdown.
        //--------------------------------------------------------------

        LOG_TRACE("Shutting down application: {0}", ApplicationName.c_str());
        
        Shutdown();
        
        GEngine->Shutdown();
        Memory::Delete(GEngine);
        
        MainWindow->Shutdown();
        Memory::Delete(MainWindow);
        
        return 0;
    }

    bool FApplication::HasAnyFlags(EApplicationFlags Flags)
    {
        return (ApplicationFlags & static_cast<uint32>(Flags)) != 0;
    }

    void FApplication::WindowResized(FWindow* Window, const glm::uvec2& Extent)
    {
        if (!Window->IsWindowMinimized())
        {
            GEngine->SetEngineViewportSize(Extent);

            OnWindowResized(Window, Extent);
        }
    }

    void FApplication::RequestExit()
    {
        Instance->bExitRequested = true;
    }

    void FApplication::PreInitStartup()
    {
        Paths::InitializePaths();
        Paths::Mount("engine://", Paths::GetEngineContentDirectory());
    }

    bool FApplication::CreateApplicationWindow()
    {
        FWindowSpecs AppWindowSpecs;
        AppWindowSpecs.Title = ApplicationName.c_str();

        FWindow::OnWindowResized.AddMember(this, &FApplication::WindowResized);
        
        MainWindow = FWindow::Create(AppWindowSpecs);
        MainWindow->Init();
        
        Windowing::SetPrimaryWindowHandle(MainWindow);
        
        return true;
    }

    
    bool FApplication::FatalError(const FString& Error)
    {
        return false;
    }
    
}
