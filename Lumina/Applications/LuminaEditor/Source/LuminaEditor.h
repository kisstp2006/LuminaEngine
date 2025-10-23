#pragma once

#include "Core/Application/Application.h"
#include "Memory/SmartPtr.h"


namespace Lumina
{
    class FProject;
}

namespace Lumina
{
    class FCamera;
    class FEditorSettings;
    class FEditorLayer;
    class FEditorPanel;

    inline class FEditorEngine* GEditorEngine = nullptr;
    
    class FEditorEngine : public FEngine
    {
    public:

        bool Init(FApplication* App) override;
        bool Shutdown() override;
        IDevelopmentToolUI* CreateDevelopmentTools() override;

        FProject& GetProject() const { return *Project.get(); }
    
    private:
        
        TUniquePtr<FProject> Project;

    };
    

    class LuminaEditor : public FApplication
    {
    public:
    
        LuminaEditor();

        bool Initialize(int argc, char** argv) override;
        FEngine* CreateEngine() override;
        
        bool ApplicationLoop() override;
        
        void CreateProject();
        void OpenProject();

        void RenderDeveloperTools(const FUpdateContext& UpdateContext) override;
        
        void Shutdown() override;
    
    private:
        
        
    };

    
    
}
