#pragma once

#include <imgui/imgui.h>
#include "Events/EventProcessor.h"
#include "nlohmann/json.hpp"
#include "Tools/EditorToolContext.h"
#include "Tools/EditorToolModal.h"
#include "Tools/Transactions/CoreTransactionTypes.h"
#include "Tools/Transactions/Transaction.h"
#include "Tools/UI/DevelopmentToolUI.h"
#include "Tools/UI/ImGui/ImGuiX.h"



namespace Lumina
{
    class FGamePreviewTool;
    class FEditorToolModal;
    class FContentBrowserEditorTool;
    class FRendererInfoEditorTool;
    class FPrimitiveDrawManager;
    class FConsoleLogEditorTool;
    class FWorldEditorTool;
    class FEditorTool;
}


namespace Lumina
{
    class FEditorUI : public IDevelopmentToolUI, public IEditorToolContext
    {
    public:

        enum class EProjectManagerTab
        {
            OpenProject = 0,
            NewProject = 1
        };

        using FUITransactionManager = TTransactionManager<FDeleteAssetTransaction>;
        
        FEditorUI() = default;
        ~FEditorUI() override = default;
        LE_NO_COPYMOVE(FEditorUI);

        // Begin IEventHandler
        bool OnEvent(FEvent& Event) override;
        //~ End IEventHandler

        void Initialize(const FUpdateContext& UpdateContext) override;
        void Deinitialize(const FUpdateContext& UpdateContext) override;

        void OnStartFrame(const FUpdateContext& UpdateContext) override;
        void OnUpdate(const FUpdateContext& UpdateContext) override;
        void OnEndFrame(const FUpdateContext& UpdateContext) override;

        void DestroyTool(const FUpdateContext& UpdateContext, FEditorTool* Tool);

        void PushModal(const FString& Title, ImVec2 Size, TMoveOnlyFunction<bool()> DrawFunction) override;

        void OpenScriptEditor(FStringView ScriptPath) override;
        void OpenAssetEditor(const FGuid& AssetGUID) override;
        void OnDestroyAsset(CObject* InAsset) override;
        
        
        template<typename T, typename... Args>
        requires eastl::is_base_of_v<FEditorTool, T> && eastl::is_constructible_v<T, Args...>
        T* CreateTool(Args&&... args);
        
        void VerifyDirtyPackages();
    
    private:

        void EditorToolLayoutCopy(FEditorTool* SourceTool);

        /** Returns false if the tool wants to close */
        bool SubmitToolMainWindow(const FUpdateContext& UpdateContext, FEditorTool* EditorTool, ImGuiID TopLevelDockspaceID);
        void DrawToolContents(const FUpdateContext& UpdateContext, FEditorTool* Tool);
        

        void CreateGameViewportTool(const FUpdateContext& UpdateContext);
        void DestroyGameViewportTool(const FUpdateContext& UpdateContext);

        void DrawScripts();
        void DrawObjectList();
        void DrawMemoryDialog();
        void DrawTitleBarMenu(const FUpdateContext& UpdateContext);
        void DrawTitleBarInfoStats(const FUpdateContext& UpdateContext);
        void DrawFileMenu();
        void DrawProjectMenu();
        void DrawToolsMenu();
        void DrawHelpMenu();
        void OpenProjectManagerDialog(EProjectManagerTab DefaultTab);
        void ConfigSettingsDialog();
        
        void OnProjectLoaded();

        void AssetRegistryDialog();
        
        void HandleUserInput(const FUpdateContext& UpdateContext);
    
    private:

        FUITransactionManager                           TransactionManager;
        FEditorModalManager                             ModalManager;
            
        ImGuiX::ApplicationTitleBar                     TitleBar;
        ImGuiWindowClass                                EditorWindowClass;

        FGamePreviewTool*                               GamePreviewTool = nullptr;
        FWorldEditorTool*                               WorldEditorTool = nullptr;

        FConsoleLogEditorTool*                          ConsoleLogTool = nullptr;
        FContentBrowserEditorTool*                      ContentBrowser = nullptr;
        TVector<FEditorTool*>                           EditorTools;
        FEditorTool*                                    LastActiveTool = nullptr;
        FString                                         FocusTargetWindowName; // If this is set we need to switch focus to this window

        THashMap<CObject*, FEditorTool*>                ActiveAssetTools;
        TQueue<FEditorTool*>                            ToolsPendingAdd;
        TQueue<FEditorTool*>                            ToolsPendingDestroy;
        
        bool                                            bShowContributors = false;
        bool                                            bShowLuminaInfo = false;
        bool                                            bShowDearImGuiDemoWindow = false;
        bool                                            bShowImGuiStyleEditor = false;
        bool                                            bShowImPlotDemoWindow = false;
        bool                                            bShowObjectDebug = false;
        bool                                            bShowRenderDebug = false;
        bool                                            bShowScriptsDebug = false;

        float                                           SmoothedFPS = 60.0f;
        float                                           SmoothedFrameTime = 16.67f;
        static constexpr float                          FPSSmoothingFactor = 0.01f;
        static constexpr float                          ObjectSmoothingFactor = 0.05f;

        EProjectManagerTab CurrentProjectManagerTab = EProjectManagerTab::OpenProject;

    };

    template <typename T, typename ... Args>
    requires eastl::is_base_of_v<FEditorTool, T> && eastl::is_constructible_v<T, Args...>
    T* FEditorUI::CreateTool(Args&&... args)
    {
        T* NewTool = Memory::New<T>(std::forward<Args>(args)...);
        NewTool->Initialize();
        ToolsPendingAdd.push(NewTool);
        return NewTool;
    }
}
