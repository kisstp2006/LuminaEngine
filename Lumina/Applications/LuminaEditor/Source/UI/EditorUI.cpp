#include "EditorUI.h"

#include "Core/Application/Application.h"
#include "imgui.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Core/Object/Class.h"
#include "Core/Object/Object.h"
#include "Memory/Memory.h"
#include "Project/Project.h"
#include "Renderer/RenderContext.h"
#include "World/Entity/Systems/EditorEntityMovementSystem.h"
#include "Tools/ConsoleLogEditorTool.h"
#include "Tools/ContentBrowserEditorTool.h"
#include "Tools/EditorTool.h"
#include "Tools/EditorToolModal.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"
#include "Tools/UI/ImGui/ImGuiX.h"
#include "Tools/WorldEditorTool.h"
#include "Tools/AssetEditors/MaterialEditor/MaterialEditorTool.h"
#include "Tools/UI/ImGui/imfilebrowser.h"
#include <Assets/AssetHeader.h>
#include <client/TracyProfiler.hpp>
#include "implot.h"
#include "LuminaEditor.h"
#include "assets/assettypes/archetype/archetype.h"
#include "Assets/AssetTypes/Material/Material.h"
#include "Assets/AssetTypes/Material/MaterialInstance.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Object/Cast.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/Package/Package.h"
#include "Core/Profiler/Profile.h"
#include "Core/Reflection/PropertyCustomization/PropertyCustomization.h"
#include "EASTL/sort.h"
#include "Platform/Process/PlatformProcess.h"
#include "Properties/Customizations/CoreTypeCustomization.h"
#include "Renderer/RenderDocImpl.h"
#include "Renderer/RenderManager.h"
#include "Renderer/RHIGlobals.h"
#include "Renderer/ShaderCompiler.h"
#include "Tools/GamePreviewTool.h"
#include "Tools/AssetEditors/ArchetypeEditor/ArchetypeEditorTool.h"
#include "Tools/AssetEditors/MaterialEditor/MaterialInstanceEditorTool.h"
#include "Tools/AssetEditors/MeshEditor/MeshEditorTool.h"
#include "Tools/AssetEditors/TextureEditor/TextureEditorTool.h"
#include "Tools/Import/ImportHelpers.h"
#include "Tools/UI/ImGui/ImGuiRenderer.h"
#include "World/WorldManager.h"
#include "World/Scene/RenderScene/RenderScene.h"

namespace Lumina
{
    FEditorUI::FEditorUI()
    {
    }

    FEditorUI::~FEditorUI()
    {
    }

    void FEditorUI::Initialize(const FUpdateContext& UpdateContext)
    {
        PropertyCustomizationRegistry = Memory::New<FPropertyCustomizationRegistry>();
        PropertyCustomizationRegistry->RegisterPropertyCustomization(TBaseStructure<glm::vec2>::Get()->GetName(), [this]()
        {
            return FVec2PropertyCustomization::MakeInstance();
        });
        PropertyCustomizationRegistry->RegisterPropertyCustomization(TBaseStructure<glm::vec3>::Get()->GetName(), [this]()
        {
            return FVec3PropertyCustomization::MakeInstance();
        });
        PropertyCustomizationRegistry->RegisterPropertyCustomization(TBaseStructure<glm::vec4>::Get()->GetName(), [this]()
        {
            return FVec4PropertyCustomization::MakeInstance();
        });
        PropertyCustomizationRegistry->RegisterPropertyCustomization(TBaseStructure<FTransform>::Get()->GetName(), [this]()
        {
            return FTransformPropertyCustomization::MakeInstance();
        });



        
        EditorWindowClass.ClassId = ImHashStr("EditorWindowClass");
        EditorWindowClass.DockingAllowUnclassed = false;
        EditorWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        EditorWindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoTaskBarIcon;
        EditorWindowClass.ParentViewportId = 0; // Top level window
        EditorWindowClass.DockingAlwaysTabBar = true;

        CWorld* NewWorld = NewObject<CWorld>();
        WorldEditorTool = CreateTool<FWorldEditorTool>(this, NewWorld);
        
        WorldEditorTool->GetOnPreviewStartRequestedDelegate().AddLambda([this]
        {
            if (CWorld* PreviewWorld = CWorld::DuplicateWorldForPIE(WorldEditorTool->GetWorld()))
            {
                PreviewWorld->SetPaused(false);
                GamePreviewTool = CreateTool<FGamePreviewTool>(this, PreviewWorld);
                WorldEditorTool->NotifyPlayInEditorStart();
            }
        });

        WorldEditorTool->GetOnPreviewStopRequestedDelegate().AddLambda([this]
        {
            ToolsPendingDestroy.push(GamePreviewTool);
        });

        
        ConsoleLogTool = CreateTool<FConsoleLogEditorTool>(this);
        ContentBrowser = CreateTool<FContentBrowserEditorTool>(this);
    }

    void FEditorUI::Deinitialize(const FUpdateContext& UpdateContext)
    {
        while (!EditorTools.empty())
        {
            // Pops internally.
            DestroyTool(UpdateContext, EditorTools[0]);
        }

        
        WorldEditorTool = nullptr;
        ConsoleLogTool = nullptr;
    }

    void FEditorUI::OnStartFrame(const FUpdateContext& UpdateContext)
    {
        LUMINA_PROFILE_SCOPE();
        Assert(UpdateContext.GetUpdateStage() == EUpdateStage::FrameStart)

        auto TitleBarLeftContents = [this, &UpdateContext] ()
        {
            DrawTitleBarMenu(UpdateContext);
        };

        auto TitleBarRightContents = [this, &UpdateContext] ()
        {
            DrawTitleBarInfoStats(UpdateContext);
        };

        TitleBar.Draw(TitleBarLeftContents, 400, TitleBarRightContents, 230);
        
        const ImGuiID DockspaceID = ImGui::GetID("EditorDockSpace");

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorDockSpaceWindow", nullptr, WindowFlags);
        ImGui::PopStyleVar(3);
        {
            // Create initial layout
            if (!ImGui::DockBuilderGetNode(DockspaceID))
            {
                ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(DockspaceID, ImGui::GetContentRegionAvail());

                ImGuiID TopDockID = 0, BottomDockID = 0;
                ImGui::DockBuilderSplitNode(DockspaceID, ImGuiDir_Down, 0.3f, &BottomDockID, &TopDockID);
                
                ImGui::DockBuilderFinish(DockspaceID);

                ImGui::DockBuilderDockWindow(WorldEditorTool->GetToolName().c_str(), TopDockID);
                ImGui::DockBuilderDockWindow(ContentBrowser->GetToolName().c_str(), BottomDockID);
                ImGui::DockBuilderDockWindow(ConsoleLogTool->GetToolName().c_str(), BottomDockID);
            }

            // Create the actual dock space
            ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0);
            ImGui::DockSpace(DockspaceID, viewport->WorkSize, 0, &EditorWindowClass);
            ImGui::PopStyleVar();
        }
        
        
        ImGui::End();
        

        if (!FocusTargetWindowName.empty())
        {
            ImGuiWindow* Window = ImGui::FindWindowByName(FocusTargetWindowName.c_str());
            if (Window == nullptr || Window->DockNode == nullptr || Window->DockNode->TabBar == nullptr)
            {
                FocusTargetWindowName.clear();
                return;
            }

            ImGuiID TabID = 0;
            for (int i = 0; i < Window->DockNode->TabBar->Tabs.size(); ++i)
            {
                ImGuiTabItem* pTab = &Window->DockNode->TabBar->Tabs[i];
                if (pTab->Window->ID == Window->ID)
                {
                    TabID = pTab->ID;
                    break;
                }
            }

            if (TabID != 0)
            {
                Window->DockNode->TabBar->NextSelectedTabId = TabID;
                ImGui::SetWindowFocus(FocusTargetWindowName.c_str());
            }

            FocusTargetWindowName.clear();
            
        }

        if (bShowLuminaInfo)
        {
            ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));

            if (ImGui::Begin("About Lumina Engine", &bShowLuminaInfo, ImGuiWindowFlags_NoCollapse))
            {
                // Hero Section
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "LUMINA ENGINE");
                ImGui::PopFont();

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("A modern, high-performance game engine built with Vulkan.");
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                // Engine Information Section
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Engine Information");
                ImGui::Spacing();

                if (ImGui::BeginTable("##EngineInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX))
                {
                    ImGui::TableSetupColumn("##Property", ImGuiTableColumnFlags_WidthFixed, 180);
                    ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);

                    // Version
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("Version");
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s", LUMINA_VERSION);

                    // Rendering API
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("Rendering API");
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("Vulkan 1.3");

                    // Build Configuration
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("Build Configuration");
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
#ifdef LUMINA_DEBUG
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Debug");
#elif defined(LUMINA_RELEASE)
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Release");
#else
                    ImGui::Text("Development");
#endif

                    // Platform
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("Platform");
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
#ifdef _WIN32
                    ImGui::Text("Windows x64");
#elif defined(__linux__)
                    ImGui::Text("Linux x64");
#elif defined(__APPLE__)
                    ImGui::Text("macOS");
#endif

                    // Compiler
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("Compiler");
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
#ifdef _MSC_VER
                    ImGui::Text("MSVC %d", _MSC_VER);
#elif defined(__clang__)
                    ImGui::Text("Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
                    ImGui::Text("GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                // Core Features Section
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Core Features");
                ImGui::Spacing();

                ImGui::BulletText("Physically Based Rendering (PBR)");
                ImGui::BulletText("Advanced Material System");
                ImGui::BulletText("Multi-threaded Asset Pipeline");
                ImGui::BulletText("Hot-reload Shader Compilation");

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                // Contributors Section
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Development Team");
                ImGui::Spacing();

                if (ImGui::BeginTable("##ContributorsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX))
                {
                    ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthFixed, 180);
                    ImGui::TableSetupColumn("##Role", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Dr. Elliot");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("Lead Developer & Engine Architect");
                    ImGui::PopStyleColor();

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Footer
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::Text("Licensed under the MIT License");
                ImGui::Text("Copyright (c) 2025 Lumina Engine Contributors");
                ImGui::PopStyleColor();

                ImGui::Spacing();

                // Action Buttons
                ImGui::Separator();
                ImGui::Spacing();

                float buttonWidth = 150.0f;
                float availWidth = ImGui::GetContentRegionAvail().x;
                ImGui::SetCursorPosX((availWidth - buttonWidth * 2 - ImGui::GetStyle().ItemSpacing.x) * 0.5f);

                if (ImGui::Button("Documentation", ImVec2(buttonWidth, 0)))
                {
                    Platform::LaunchURL(TEXT("https://github.com/MrDrElliot/LuminaEngine"));
                }

                ImGui::SameLine();

                if (ImGui::Button("GitHub", ImVec2(buttonWidth, 0)))
                {
                    Platform::LaunchURL(TEXT("https://github.com/MrDrElliot/LuminaEngine"));
                }
            }

            ImGui::End();
            ImGui::PopStyleVar();
        }

        if (bShowDearImGuiDemoWindow)
        {
            ImGui::ShowDemoWindow(&bShowDearImGuiDemoWindow);
        }

        if (bShowImPlotDemoWindow)
        {
            ImPlot::ShowDemoWindow(&bShowImPlotDemoWindow);
        }

        if (bShowRenderDebug)
        {
            UpdateContext.GetSubsystem<FRenderManager>()->GetImGuiRenderer()->DrawRenderDebugInformationWindow(&bShowRenderDebug, UpdateContext);
        }

        if (bShowObjectDebug)
        {
            // State management
            static FString SearchFilter;
            static FString ClassFilter;
            static bool bSortByName = true;
            static bool bShowOnlyActive = false;
            static int SelectedObjectIndex = -1;
            static FString SelectedPackage;
            static char SearchBuffer[256] = "";
            static char ClassFilterBuffer[256] = "";
            
            ImGui::SetNextWindowSize(ImVec2(1200.0f, 800.0f), ImGuiCond_FirstUseEver);
            
            // Custom styling
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 0.45f));
            
            FString TitleText = "Object Browser";
            
            if (ImGui::Begin(TitleText.c_str(), (bool*)&bShowObjectDebug, ImGuiWindowFlags_MenuBar))
            {
                uint32 TotalObjects = GObjectArray.GetNumAliveObjects();
                
                // Menu Bar
                if (ImGui::BeginMenuBar())
                {
                    if (ImGui::BeginMenu("View"))
                    {
                        ImGui::MenuItem("Sort by Name", nullptr, &bSortByName);
                        ImGui::MenuItem("Show Only Active", nullptr, &bShowOnlyActive);
                        ImGui::EndMenu();
                    }
                    
                    if (ImGui::BeginMenu("Tools"))
                    {
                        if (ImGui::MenuItem("Clear Filters"))
                        {
                            SearchBuffer[0] = '\0';
                            ClassFilterBuffer[0] = '\0';
                            SearchFilter = "";
                            ClassFilter = "";
                        }
                        if (ImGui::MenuItem("Collapse All Packages"))
                        {
                            // Reset selection state
                            SelectedPackage = "";
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenuBar();
                }
                
                // Top stats bar with gradient background
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
                ImGui::BeginChild("##StatsBar", ImVec2(0, 70.0f), true, ImGuiWindowFlags_NoScrollbar);
                {
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assume larger font
                    
                    // Stats display
                    ImGui::Columns(3, nullptr, false);
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                    ImGui::TextWrapped("TOTAL OBJECTS");
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::Text("%d", TotalObjects);
                    ImGui::PopStyleColor();
                    
                    ImGui::NextColumn();
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
                    ImGui::TextWrapped("PACKAGES");
                    ImGui::PopStyleColor();
                    
                    // Count packages (will calculate below)
                    static int PackageCount = 0;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::Text("%d", PackageCount);
                    ImGui::PopStyleColor();
                    
                    ImGui::NextColumn();
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                    ImGui::TextWrapped("FILTERED");
                    ImGui::PopStyleColor();
                    
                    static int FilteredCount = 0;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::Text("%d", FilteredCount);
                    ImGui::PopStyleColor();
                    
                    ImGui::Columns(1);
                    ImGui::PopFont();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                
                // Filter panel
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.15f, 1.0f));
                ImGui::BeginChild("##FilterPanel", ImVec2(0, 90.0f), true, ImGuiWindowFlags_NoScrollbar);
                {
                    ImGui::Text("FILTERS");
                    ImGui::Separator();
                    
                    ImGui::Columns(2, nullptr, false);
                    
                    // Search filter
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextWithHint("##Search", "Search objects...", SearchBuffer, IM_ARRAYSIZE(SearchBuffer)))
                    {
                        SearchFilter = SearchBuffer;
                    }
                    
                    ImGui::NextColumn();
                    
                    // Class filter
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextWithHint("##ClassFilter", "Filter by class...", ClassFilterBuffer, IM_ARRAYSIZE(ClassFilterBuffer)))
                    {
                        ClassFilter = ClassFilterBuffer;
                    }
                    
                    ImGui::Columns(1);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                
                // Main content area - split view
                ImGui::BeginChild("##MainContent", ImVec2(0, 0), false);
                {
                    // Left panel - Package tree
                    ImGui::BeginChild("##PackageTree", ImVec2(ImGui::GetContentRegionAvail().x * 0.35f, 0), true);
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                        ImGui::Text("PACKAGES");
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                        
                        // Build package map with filtering
                        THashMap<FString, TVector<CObject*>> PackageToObjects;
                        uint32 FilteredCount = 0;
                        
                        for (TObjectIterator<CObject> It; It; ++It)
                        {
                            CObject* Object = *It;
                            if (Object == nullptr)
                            {
                                continue;
                            }

                            // Apply filters
                            FString ObjectName = Object->GetName().ToString();
                            FString ClassName = Object->GetClass() ? Object->GetClass()->GetName().ToString() : "None";
                            
                            bool bPassesFilter = true;
                            
                            if (!SearchFilter.empty())
                            {
                                if (ObjectName.find(SearchFilter) == FString::npos)
                                {
                                    bPassesFilter = false;
                                }
                            }
                            
                            if (ClassFilter.empty() && bPassesFilter && ClassName.find(ClassFilter) == FString::npos)
                            {
                                bPassesFilter = false;
                            }
                            
                            if (bShowOnlyActive && bPassesFilter)
                            {
                                // Add your active check logic here
                            }
                            
                            if (bPassesFilter)
                            {
                                FString PackageName = Object->GetPackage() ? Object->GetPackage()->GetName().ToString() : "None";
                                PackageToObjects[PackageName].push_back(Object);
                                FilteredCount++;
                            }
                        }
                        
                        for (auto& Pair : PackageToObjects)
                        {
                            const FString& PackageName = Pair.first;
                            TVector<CObject*>& Objects = Pair.second;
                            
                            if (bSortByName)
                            {
                                eastl::sort(Objects.begin(), Objects.end(), [](CObject* A, CObject* B)
                                {
                                    return A->GetName().ToString() < B->GetName().ToString();
                                });
                            }
                            
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.25f, 0.30f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.35f, 0.45f, 1.0f));
                            
                            FString NodeLabel = PackageName + " (" + eastl::to_string(Objects.size()) + ")";
                            bool bIsSelected = (SelectedPackage == PackageName);
                            
                            if (ImGui::Selectable(NodeLabel.c_str(), bIsSelected, ImGuiSelectableFlags_AllowDoubleClick))
                            {
                                SelectedPackage = PackageName;
                            }
                            
                            ImGui::PopStyleColor(2);
                        }
                    }
                    ImGui::EndChild();
                    
                    ImGui::SameLine();
                    
                    // Right panel - Object details
                    ImGui::BeginChild("##ObjectDetails", ImVec2(0, 0), true);
                    {
                        if (!SelectedPackage.empty())
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                            ImGui::Text("OBJECTS IN: %s", SelectedPackage.c_str());
                            ImGui::PopStyleColor();
                            ImGui::Separator();
                            
                            // Build filtered objects for selected package
                            THashMap<FString, TVector<CObject*>> PackageToObjects;
                            
                            for (TObjectIterator<CObject> It; It; ++It)
                            {
                                CObject* Object = *It;
                                if (Object == nullptr)
                                {
                                    continue;
                                }

                                FString ObjectName = Object->GetName().ToString();
                                FString ClassName = Object->GetClass() ? Object->GetClass()->GetName().ToString() : "None";
                                
                                bool bPassesFilter = true;
                                
                                if (!SearchFilter.empty() && ObjectName.find(SearchFilter) == FString::npos)
                                {
                                    bPassesFilter = false;
                                }
                                
                                if (ClassFilter.empty() && bPassesFilter && ClassName.find(ClassFilter) == FString::npos)
                                {
                                    bPassesFilter = false;
                                }
                                
                                if (bPassesFilter)
                                {
                                    FString PackageName = Object->GetPackage() ? Object->GetPackage()->GetName().ToString() : "None";
                                    if (PackageName == SelectedPackage)
                                    {
                                        PackageToObjects[PackageName].push_back(Object);
                                    }
                                }
                            }
                            
                            if (PackageToObjects.find(SelectedPackage) != PackageToObjects.end())
                            {
                                TVector<CObject*>& Objects = PackageToObjects[SelectedPackage];
                                
                                // Advanced table with alternating colors
                                if (ImGui::BeginTable("##ObjectTable", 4, 
                                    ImGuiTableFlags_RowBg | 
                                    ImGuiTableFlags_Borders | 
                                    ImGuiTableFlags_Resizable | 
                                    ImGuiTableFlags_ScrollY |
                                    ImGuiTableFlags_Sortable |
                                    ImGuiTableFlags_SizingStretchProp))
                                {
                                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.4f);
                                    ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                                    ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                                    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch, 0.1f);
                                    ImGui::TableSetupScrollFreeze(0, 1);
                                    ImGui::TableHeadersRow();
                                    
                                    ImGuiListClipper Clipper;
                                    Clipper.Begin(Objects.size());
                                    
                                    while (Clipper.Step())
                                    {
                                        for (int i = Clipper.DisplayStart; i < Clipper.DisplayEnd; i++)
                                        {
                                            CObject* Object = Objects[i];
                                            ImGui::TableNextRow();
                                            
                                            bool bIsRowSelected = (SelectedObjectIndex == i);
                                            
                                            // Name column
                                            ImGui::TableSetColumnIndex(0);
                                            ImGuiSelectableFlags Flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
                                            
                                            if (ImGui::Selectable(Object->GetName().ToString().c_str(), bIsRowSelected, Flags))
                                            {
                                                SelectedObjectIndex = i;
                                                
                                                if (ImGui::IsMouseDoubleClicked(0))
                                                {
                                                    // Double-click action: copy to clipboard or jump to object
                                                    ImGui::SetClipboardText(Object->GetName().ToString().c_str());
                                                }
                                            }
                                            
                                            // Context menu
                                            if (ImGui::BeginPopupContextItem())
                                            {
                                                if (ImGui::MenuItem("Copy Name"))
                                                {
                                                    ImGui::SetClipboardText(Object->GetName().ToString().c_str());
                                                }
                                                if (ImGui::MenuItem("Copy Address"))
                                                {
                                                    char Buffer[32];
                                                    snprintf(Buffer, sizeof(Buffer), "0x%p", Object);
                                                    ImGui::SetClipboardText(Buffer);
                                                }
                                                ImGui::EndPopup();
                                            }
                                            
                                            // Class column
                                            ImGui::TableSetColumnIndex(1);
                                            if (Object->GetClass())
                                            {
                                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
                                                ImGui::TextUnformatted(Object->GetClass()->GetName().ToString().c_str());
                                                ImGui::PopStyleColor();
                                            }
                                            else
                                            {
                                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                                                ImGui::TextUnformatted("None");
                                                ImGui::PopStyleColor();
                                            }
                                            
                                            // Flags column
                                            ImGui::TableSetColumnIndex(2);
                                            FInlineString FlagsStr = ObjectFlagsToString(Object->GetFlags());
                                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
                                            ImGui::TextUnformatted(FlagsStr.c_str());
                                            ImGui::PopStyleColor();
                                            
                                            // Address column
                                            ImGui::TableSetColumnIndex(3);
                                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                                            ImGui::Text("0x%p", Object);
                                            ImGui::PopStyleColor();
                                        }
                                    }
                                    
                                    ImGui::EndTable();
                                }
                            }
                        }
                        else
                        {
                            // No package selected
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                            ImVec2 TextSize = ImGui::CalcTextSize("Select a package to view objects");
                            ImVec2 WindowSize = ImGui::GetWindowSize();
                            ImGui::SetCursorPos(ImVec2((WindowSize.x - TextSize.x) * 0.5f, (WindowSize.y - TextSize.y) * 0.5f));
                            ImGui::Text("Select a package to view objects");
                            ImGui::PopStyleColor();
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
            
            ImGui::End();
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
        }

        if (GEngine->IsCloseRequested())
        {
            static bool IsVerifyingPackages = false;
            if (IsVerifyingPackages == false)
            {
                IsVerifyingPackages = true;
                VerifyDirtyPackages();
            }

            if (ModalManager.HasModal())
            {
                GEngine->SetEngineReadyToClose(false);
            }
        }
        
        FEditorTool* ToolToClose = nullptr;
        
        for (FEditorTool* Tool : EditorTools)
        {
            if (!SubmitToolMainWindow(UpdateContext, Tool, DockspaceID))
            {
                ToolToClose = Tool;
            }
        }

        for (FEditorTool* Tool : EditorTools)
        {
            if (Tool == ToolToClose)
            {
                continue;
            }
            
            DrawToolContents(UpdateContext, Tool);
        }

        
        if (ToolToClose)
        {
            ToolsPendingDestroy.push(ToolToClose);
        }

        while (!ToolsPendingDestroy.empty())
        {
            FEditorTool* Tool = ToolsPendingDestroy.front();
            ToolsPendingDestroy.pop();

            DestroyTool(UpdateContext, Tool);
        }
        
        while (!ToolsPendingAdd.empty())
        {
            FEditorTool* NewTool = ToolsPendingAdd.front();
            ToolsPendingAdd.pop();

            EditorTools.push_back(NewTool);
        }

        if (!GEditorEngine->GetProject().HasLoadedProject())
        {
            OpenProjectDialog();
        }
        

        ModalManager.DrawDialogue(UpdateContext);
        
    }

    void FEditorUI::OnUpdate(const FUpdateContext& UpdateContext)
    {
        LUMINA_PROFILE_SCOPE();
        
        for (FEditorTool* Tool : EditorTools)
        {
            if (Tool->HasWorld())
            {
                Tool->WorldUpdate(UpdateContext);
            }
        }
    }

    void FEditorUI::OnEndFrame(const FUpdateContext& UpdateContext)
    {
        LUMINA_PROFILE_SCOPE();

        for (FEditorTool* Tool : EditorTools)
        {
            Tool->EndFrame();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            TransactionManager.Redo();
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            TransactionManager.Undo();
        }

    }
    
    void FEditorUI::DestroyTool(const FUpdateContext& UpdateContext, FEditorTool* Tool)
    {
        auto Itr = eastl::find(EditorTools.begin(), EditorTools.end(), Tool);
        Assert(Itr != EditorTools.end())

        EditorTools.erase(Itr);
        for (auto MapItr = ActiveAssetTools.begin(); MapItr != ActiveAssetTools.end(); ++MapItr)
        {
            if (MapItr->second == Tool)
            {
                ActiveAssetTools.erase(MapItr);
                break;
            }
        }

        if (Tool == GamePreviewTool)
        {
            WorldEditorTool->NotifyPlayInEditorStop();
            GamePreviewTool = nullptr;
        }
        
        Tool->Deinitialize(UpdateContext);
        Memory::Delete(Tool);
    }

    void FEditorUI::PushModal(const FString& Title, ImVec2 Size, TFunction<bool(const FUpdateContext&)> DrawFunction)
    {
        ModalManager.CreateDialogue(Title, Size, DrawFunction);
    }
    
    void FEditorUI::OpenAssetEditor(CObject* InAsset)
    {
        /** Temp garbage for now */
        if (InAsset != nullptr && ActiveAssetTools.find(InAsset) == ActiveAssetTools.end())
        {
            FEditorTool* NewTool = nullptr;
            if (InAsset->IsA<CMaterial>())
            {
                NewTool = CreateTool<FMaterialEditorTool>(this, InAsset);
            }
            else if (InAsset->IsA<CTexture>())
            {
                NewTool = CreateTool<FTextureEditorTool>(this, InAsset);
            }
            else if (InAsset->IsA<CStaticMesh>())
            {
                NewTool = CreateTool<FMeshEditorTool>(this, InAsset);
            }
            else if (InAsset->IsA<CMaterialInstance>())
            {
                NewTool = CreateTool<FMaterialInstanceEditorTool>(this, InAsset);
            }
            else if (InAsset->IsA<CArchetype>())
            {
                NewTool = CreateTool<FArchetypeEditorTool>(this, InAsset);
            }
            else if (InAsset->IsA<CWorld>())
            {
                WorldEditorTool->SetWorld(Cast<CWorld>(InAsset));
            }
            
            ActiveAssetTools.insert_or_assign(InAsset, NewTool);
        }
    }

    void FEditorUI::OnDestroyAsset(CObject* InAsset)
    {
        if (ActiveAssetTools.find(InAsset) != ActiveAssetTools.end())
        {
            ToolsPendingDestroy.push(ActiveAssetTools.at(InAsset));
        }
    }

    void FEditorUI::EditorToolLayoutCopy(FEditorTool* SourceTool)
    {
        LUMINA_PROFILE_SCOPE();

        ImGuiID sourceToolID = SourceTool->GetPrevDockspaceID();
        ImGuiID destinationToolID = SourceTool->GetCurrDockspaceID();
        Assert(sourceToolID != 0 && destinationToolID != 0)
        
        // Helper to build an array of strings pointer into the same contiguous memory buffer.
        struct ContiguousStringArrayBuilder
        {
            void AddEntry( const char* data, size_t dataLength )
            {
                const int32 bufferSize = (int32_t) m_buffer.size();
                m_offsets.push_back( bufferSize );
                const int32 offset = bufferSize;
                m_buffer.resize( bufferSize + (int32_t) dataLength );
                memcpy( m_buffer.data() + offset, data, dataLength );
            }

            void BuildPointerArray( ImVector<const char*>& outArray )
            {
                outArray.resize( (int32_t) m_offsets.size() );
                for (int32 n = 0; n < (int32) m_offsets.size(); n++)
                {
                    outArray[n] = m_buffer.data() + m_offsets[n];
                }
            }

            TFixedVector<char, 100>       m_buffer;
            TFixedVector<int32, 100>    m_offsets;
        };

        ContiguousStringArrayBuilder namePairsBuilder;

        for (FEditorTool::FToolWindow* Window : SourceTool->GetToolWindows())
        {
            const FInlineString sourceToolWindowName = FEditorTool::GetToolWindowName(Window->Name.c_str(), sourceToolID);
            const FInlineString destinationToolWindowName = FEditorTool::GetToolWindowName(Window->Name.c_str(), destinationToolID);
            namePairsBuilder.AddEntry( sourceToolWindowName.c_str(), sourceToolWindowName.length() + 1 );
            namePairsBuilder.AddEntry( destinationToolWindowName.c_str(), destinationToolWindowName.length() + 1 );
        }

        // Perform the cloning
        if (ImGui::DockContextFindNodeByID( ImGui::GetCurrentContext(), sourceToolID))
        {
            // Build the same array with char* pointers at it is the input of DockBuilderCopyDockspace() (may change its signature?)
            ImVector<const char*> windowRemapPairs;
            namePairsBuilder.BuildPointerArray(windowRemapPairs);

            ImGui::DockBuilderCopyDockSpace(sourceToolID, destinationToolID, &windowRemapPairs);
            ImGui::DockBuilderFinish(destinationToolID);
        }
    }

    bool FEditorUI::SubmitToolMainWindow(const FUpdateContext& UpdateContext, FEditorTool* EditorTool, ImGuiID TopLevelDockspaceID)
    {
        LUMINA_PROFILE_SCOPE();
        Assert(EditorTool != nullptr)
        Assert(TopLevelDockspaceID != 0)

        bool bIsToolStillOpen = true;
        bool* bIsToolOpen = (EditorTool == WorldEditorTool) ? nullptr : &bIsToolStillOpen; // Prevent closing the map-editor editor tool
        
        // Top level editors can only be docked with each others
        ImGui::SetNextWindowClass(&EditorWindowClass);
        if (EditorTool->GetDesiredDockID() != 0)
        {
            ImGui::SetNextWindowDockID(EditorTool->GetDesiredDockID());
            EditorTool->DesiredDockID = 0;
        }
        else
        {
            ImGui::SetNextWindowDockID(TopLevelDockspaceID, ImGuiCond_FirstUseEver);
        }
        
        ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

        ImGuiWindow* CurrentWindow = ImGui::FindWindowByName(EditorTool->GetToolName().c_str());
        const bool bVisible = CurrentWindow != nullptr && !CurrentWindow->Hidden;
        
        ImVec4 VisibleColor   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 NotVisibleColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, bVisible ? VisibleColor : NotVisibleColor);
        ImGui::SetNextWindowSizeConstraints(ImVec2(128, 128), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(1024, 768), ImGuiCond_FirstUseEver);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::Begin(EditorTool->GetToolName().c_str(), bIsToolOpen, WindowFlags);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_DockHierarchy))
        {
            LastActiveTool = EditorTool;
        }
        
        // Set WindowClass based on per-document ID, so tabs from Document A are not dockable in Document B etc. We could be using any ID suiting us, e.g. &doc
        // We also set ParentViewportId to request the platform back-end to set parent/child relationship at the windowing level
        EditorTool->ToolWindowsClass.ClassId = EditorTool->GetID();
        EditorTool->ToolWindowsClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoTaskBarIcon | ImGuiViewportFlags_NoDecoration;
        EditorTool->ToolWindowsClass.ParentViewportId = ImGui::GetWindowViewport()->ID;
        EditorTool->ToolWindowsClass.DockingAllowUnclassed = true;

        // Track LocationID change so we can fork/copy the layout data according to where the window is going + reference count
        // LocationID ~~ (DockId != 0 ? DockId : DocumentID) // When we are in a loose floating window we use our own document id instead of the dock id
        EditorTool->CurrDockID = ImGui::GetWindowDockID();
        EditorTool->PrevLocationID = EditorTool->CurrLocationID;
        EditorTool->CurrLocationID = EditorTool->CurrDockID != 0 ? EditorTool->CurrDockID : EditorTool->GetID();

        // Dockspace ID ~~ Hash of LocationID + DocType
        // So all editors of a same type inside a same tab-bar will share the same layout.
        // We will also use this value as a suffix to create window titles, but we could perfectly have an indirection to allocate and use nicer names for window names (e.g. 0001, 0002).
        EditorTool->PrevDockspaceID = EditorTool->CurrDockspaceID;
        EditorTool->CurrDockspaceID = EditorTool->CalculateDockspaceID();
        LUM_ASSERT(EditorTool->CurrDockspaceID != 0)
        

        ImGui::End();

        return bIsToolStillOpen;
    }

    void FEditorUI::DrawToolContents(const FUpdateContext& UpdateContext, FEditorTool* Tool)
    {
        LUMINA_PROFILE_SCOPE();

        // This is the second Begin(), as SubmitToolMainWindow() has already done one
        // (Therefore only the p_open and flags of the first call to Begin() applies)
        ImGui::Begin(Tool->GetToolName().c_str());
        
        LUM_ASSERT(ImGui::GetCurrentWindow()->BeginCount == 2)
        
        const ImGuiID dockspaceID = Tool->GetCurrentDockspaceID();
        const ImVec2 DockspaceSize = ImGui::GetContentRegionAvail();

        if (Tool->PrevLocationID != 0 && Tool->PrevLocationID != Tool->CurrLocationID)
        {
            int PrevDockspaceRefCount = 0;
            int CurrDockspaceRefCount = 0;
            for (FEditorTool* OtherTool : EditorTools)
            {
                if (OtherTool->CurrDockspaceID == Tool->PrevDockspaceID)
                {
                    PrevDockspaceRefCount++;
                }
                else if (OtherTool->CurrDockspaceID == Tool->CurrDockspaceID)
                {
                    CurrDockspaceRefCount++;
                }
            }

            // Fork or overwrite settings
            // FIXME: should be able to do a "move window but keep layout" if CurrDockspaceRefCount > 1.
            // FIXME: when moving, delete settings of old windows
            EditorToolLayoutCopy(Tool);

            if (PrevDockspaceRefCount == 0)
            {
                ImGui::DockBuilderRemoveNode(Tool->PrevDockspaceID);

                // Delete settings of old windows
                // Rely on window name to ditch their .ini settings forever.
                char windowSuffix[16];
                ImFormatString(windowSuffix, IM_ARRAYSIZE(windowSuffix), "##%08X", Tool->PrevDockspaceID);
                size_t windowSuffixLength = strlen(windowSuffix);
                ImGuiContext& g = *GImGui;
                for (ImGuiWindowSettings* settings = g.SettingsWindows.begin(); settings != nullptr; settings = g.SettingsWindows.next_chunk(settings))
                {
                    if ( settings->ID == 0 )
                    {
                        continue;
                    }
                    
                    
                    char const* pWindowName = settings->GetName();
                    size_t windowNameLength = strlen(pWindowName);
                    if (windowNameLength >= windowSuffixLength)
                    {
                        if (strcmp(pWindowName + windowNameLength - windowSuffixLength, windowSuffix) == 0) // Compare suffix
                        {
                            ImGui::ClearWindowSettings(pWindowName);
                        }
                    }
                }
            }
        }
        else if (ImGui::DockBuilderGetNode(Tool->GetCurrentDockspaceID()) == nullptr)
        {
            ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();
            dockspaceSize.x = eastl::max(dockspaceSize.x, 1.0f);
            dockspaceSize.y = eastl::max(dockspaceSize.y, 1.0f);

            ImGui::DockBuilderAddNode(Tool->GetCurrentDockspaceID(), ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(Tool->GetCurrentDockspaceID(), dockspaceSize);
            if (!Tool->IsSingleWindowTool())
            {
                Tool->InitializeDockingLayout(Tool->GetCurrentDockspaceID(), dockspaceSize);
            }
            ImGui::DockBuilderFinish(Tool->GetCurrentDockspaceID());
        }

        // FIXME-DOCK: This is a little tricky to explain, but we currently need this to use the pattern of sharing a same dockspace between tabs of a same tab bar
        bool bVisible = true;
        if (ImGui::GetCurrentWindow()->Hidden)
        {
            bVisible = false;
        }
        
        const bool bIsLastFocusedTool = (LastActiveTool == Tool);
        
        Tool->Update(UpdateContext);
        Tool->bViewportFocused = false;
        Tool->bViewportHovered = false;

        if (Tool->HasWorld())
        {
            Tool->GetWorld()->SetActive(bVisible);
        }
        
        if (!bVisible)
        {
            if (!Tool->IsSingleWindowTool())
            {
                // Keep alive document dockspace so windows that are docked into it but which visibility are not linked to the dockspace visibility won't get undocked.
                ImGui::DockSpace(dockspaceID, DockspaceSize, ImGuiDockNodeFlags_KeepAliveOnly, &Tool->ToolWindowsClass);
            }
            
            ImGui::End();
            
            return;
        }

        
        if (Tool->HasFlag(EEditorToolFlags::Tool_WantsToolbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 16));
            if (ImGui::BeginMenuBar())
            {
                Tool->DrawMainToolbar(UpdateContext);
                ImGui::EndMenuBar();
            }
            ImGui::PopStyleVar();
        }

        if (Tool->IsSingleWindowTool())
        {
            Assert(Tool->ToolWindows.size() == 1)
            Tool->ToolWindows[0]->DrawFunction(UpdateContext, bIsLastFocusedTool);
        }
        else
        {
            ImGui::DockSpace(dockspaceID, DockspaceSize, ImGuiDockNodeFlags_None, &Tool->ToolWindowsClass);
        }
    
        ImGui::End();


        if (!Tool->IsSingleWindowTool())
        {
            for (FEditorTool::FToolWindow* Window : Tool->ToolWindows)
            {
                LUMINA_PROFILE_SECTION("Setup and Draw Tool Window");

                const FInlineString ToolWindowName = FEditorTool::GetToolWindowName(Window->Name.c_str(), Tool->GetCurrentDockspaceID());

                // When multiple documents are open, floating tools only appear for focused one
                if (!bIsLastFocusedTool)
                {
                    if (ImGuiWindow* pWindow = ImGui::FindWindowByName(ToolWindowName.c_str()))
                    {
                        ImGuiDockNode* pWindowDockNode = pWindow->DockNode;
                        if (pWindowDockNode == nullptr && pWindow->DockId != 0)
                        {
                            pWindowDockNode = ImGui::DockContextFindNodeByID(ImGui::GetCurrentContext(), pWindow->DockId);
                        }
                       
                        if (pWindowDockNode == nullptr || ImGui::DockNodeGetRootNode(pWindowDockNode)->ID != dockspaceID)
                        {
                            continue;
                        }
                    }
                }
            
                if (Window->bViewport)
                {
                    LUMINA_PROFILE_SECTION("Draw Viewport");

                    constexpr ImGuiWindowFlags ViewportWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
                    ImGui::SetNextWindowClass(&Tool->ToolWindowsClass);

                    //-- Setup viewport for scene.
                    
                    ImGui::SetNextWindowSizeConstraints(ImVec2(128, 128), ImVec2(FLT_MAX, FLT_MAX));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    bool const DrawViewportWindow = ImGui::Begin(ToolWindowName.c_str(), nullptr, ViewportWindowFlags);
                    ImGui::PopStyleVar();
                
                    if (DrawViewportWindow)
                    {
                        IRenderScene* SceneRenderer = Tool->GetWorld()->GetRenderer();
                        ImTextureRef ViewportTexture = ImGuiX::ToImTextureRef(SceneRenderer->GetRenderTarget());
                        
                        Tool->bViewportFocused = ImGui::IsWindowFocused();
                        Tool->bViewportHovered = ImGui::IsWindowHovered();
                        Tool->DrawViewport(UpdateContext, ViewportTexture);
                        
                    }
                    
                    ImGui::End();
                }
                else
                {
                    LUMINA_PROFILE_SECTION("Draw Tool Window");

                    ImGuiWindowFlags ToolWindowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;

                    ImGui::SetNextWindowClass(&Tool->ToolWindowsClass);

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().WindowPadding);
                    bool const DrawToolWindow = ImGui::Begin(ToolWindowName.c_str(), &Window->bOpen, ToolWindowFlags);
                    ImGui::PopStyleVar();

                    if (DrawToolWindow)
                    {
                        const bool bToolWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_DockHierarchy);
                        Window->DrawFunction(UpdateContext, bToolWindowFocused);
                    }
                    
                    ImGui::End();
                }
                
            }
        }

        if (Tool->HasWorld() && !Tool->World->IsPlayWorld())
        {
            Tool->SetEditorCameraEnabled(Tool->bViewportFocused);
        }
    }

    void FEditorUI::CreateGameViewportTool(const FUpdateContext& UpdateContext)
    {
    }

    void FEditorUI::DestroyGameViewportTool(const FUpdateContext& UpdateContext)
    {
        
    }

    void FEditorUI::DrawMemoryDialog()
    {
        ModalManager.CreateDialogue("Memory Profiler", ImVec2(1000, 900), [this](const FUpdateContext& Ctx) -> bool
        {
            // Static state for persistent tracking
            struct MemorySnapshot
            {
                double timestamp;
                size_t processMemory;
                size_t currentMapped;
                size_t cachedMemory;
                size_t hugeAllocs;
            };
            
            static TVector<MemorySnapshot> history;
            static float updateTimer = 0.0f;
            static constexpr int maxHistoryPoints = 60;
            static bool isPaused = false;
            
            // Update history
            updateTimer += Ctx.GetDeltaTime();
            if (!isPaused && updateTimer >= 1.0f)
            {
                updateTimer = 0.0f;
                
                MemorySnapshot snapshot;
                snapshot.timestamp = ImGui::GetTime();
                snapshot.processMemory = Platform::GetProcessMemoryUsageBytes();
                snapshot.currentMapped = Memory::GetCurrentMappedMemory();
                snapshot.cachedMemory = Memory::GetCachedMemory();
                snapshot.hugeAllocs = Memory::GetCurrentHugeAllocMemory();
                
                history.push_back(snapshot);
                
                if (history.size() > maxHistoryPoints)
                {
                    history.erase(history.begin());
                }
            }
            
            // Header
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.5f, 1.0f));
            ImGui::Text(LE_ICON_CHART_LINE " Memory Profiler");
            ImGui::PopStyleColor();
            
            ImGui::SameLine(ImGui::GetWindowWidth() - 330);
            if (ImGui::Button(isPaused ? LE_ICON_PLAY " Resume" : LE_ICON_PAUSE " Pause", ImVec2(100, 0)))
            {
                isPaused = !isPaused;
            }
            
            ImGui::SameLine();
            if (ImGui::Button(LE_ICON_TRASH_CAN " Clear", ImVec2(100, 0)))
            {
                history.clear();
            }
            
            ImGui::Separator();
            ImGui::Spacing();
            
            // View tabs
            if (ImGui::BeginTabBar("MemoryTabs"))
            {
                // Overview Tab
                if (ImGui::BeginTabItem(LE_ICON_VIEW_DASHBOARD " Overview"))
                {
                    // Current stats cards
                    ImGui::BeginGroup();
                    {
                        size_t processMemory = Platform::GetProcessMemoryUsageBytes();
                        size_t currentMapped = Memory::GetCurrentMappedMemory();
                        size_t peakMapped = Memory::GetPeakMappedMemory();
                        size_t cachedMemory = Memory::GetCachedMemory();
                        
                        float cardWidth = (ImGui::GetContentRegionAvail().x - 30) / 4.0f;
                        
                        // Card 1: Process Memory
                        ImGui::BeginChild("Card1", ImVec2(cardWidth, 100), true, ImGuiWindowFlags_NoScrollbar);
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                            ImGui::Text(LE_ICON_MEMORY " Process Memory");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();
                            ImGui::Text("%s", ImGuiX::FormatSize(processMemory).c_str());
                        }
                        ImGui::EndChild();
                        
                        ImGui::SameLine();
                        
                        // Card 2: Mapped Memory
                        ImGui::BeginChild("Card2", ImVec2(cardWidth, 100), true, ImGuiWindowFlags_NoScrollbar);
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
                            ImGui::Text(LE_ICON_DATABASE " Mapped");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();
                            ImGui::Text("%s", ImGuiX::FormatSize(currentMapped).c_str());
                            float usage = peakMapped > 0 ? (float)currentMapped / peakMapped : 0.0f;
                            ImGui::ProgressBar(usage, ImVec2(-1, 0), "");
                        }
                        ImGui::EndChild();
                        
                        ImGui::SameLine();
                        
                        // Card 3: Cached Memory
                        ImGui::BeginChild("Card3", ImVec2(cardWidth, 100), true, ImGuiWindowFlags_NoScrollbar);
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                            ImGui::Text(LE_ICON_LAYERS " Cached");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();
                            ImGui::Text("%s", ImGuiX::FormatSize(cachedMemory).c_str());
                        }
                        ImGui::EndChild();
                        
                        ImGui::SameLine();
                        
                        // Card 4: Huge Allocs
                        ImGui::BeginChild("Card4", ImVec2(cardWidth, 100), true, ImGuiWindowFlags_NoScrollbar);
                        {
                            size_t hugeAllocs = Memory::GetCurrentHugeAllocMemory();
                            size_t peakHuge = Memory::GetPeakHugeAllocMemory();
                            
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                            ImGui::Text(LE_ICON_CUBE " Huge Allocs");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();
                            ImGui::Text("%s", ImGuiX::FormatSize(hugeAllocs).c_str());
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Peak: %s", ImGuiX::FormatSize(peakHuge).c_str());
                        }
                        ImGui::EndChild();
                    }
                    ImGui::EndGroup();
                    
                    ImGui::Spacing();
                    
                    // Main graph
                    if (history.size() > 1 && ImPlot::BeginPlot("Memory Usage Over Time", ImVec2(-1, 450)))
                    {
                        ImPlot::SetupAxes("Time (seconds)", "Memory (bytes)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                        ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                        
                        // Prepare data
                        TVector<double> times;
                        TVector<double> processMem, mappedMem, cachedMem, hugeMem;
                        
                        double baseTime = history[0].timestamp;
                        for (const auto& snap : history)
                        {
                            times.push_back(snap.timestamp - baseTime);
                            processMem.push_back((double)snap.processMemory);
                            mappedMem.push_back((double)snap.currentMapped);
                            cachedMem.push_back((double)snap.cachedMemory);
                            hugeMem.push_back((double)snap.hugeAllocs);
                        }
                        
                        ImPlot::SetNextLineStyle(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), 2.0f);
                        ImPlot::PlotLine("Process Memory", times.data(), processMem.data(), (int)times.size());
                        
                        ImPlot::SetNextLineStyle(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), 2.0f);
                        ImPlot::PlotLine("Mapped Memory", times.data(), mappedMem.data(), (int)times.size());
                        
                        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), 1.5f);
                        ImPlot::PlotLine("Cached Memory", times.data(), cachedMem.data(), (int)times.size());
                        
                        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), 1.5f);
                        ImPlot::PlotLine("Huge Allocs", times.data(), hugeMem.data(), (int)times.size());
                        
                        ImPlot::EndPlot();
                    }
                    else if (history.size() <= 1)
                    {
                        ImGui::BeginChild("NoData", ImVec2(-1, 450), true);
                        ImGui::SetCursorPosY(200);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                        float textWidth = ImGui::CalcTextSize("Collecting data...").x;
                        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) / 2);
                        ImGui::Text("Collecting data...");
                        ImGui::PopStyleColor();
                        ImGui::EndChild();
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // Detailed Statistics Tab
                if (ImGui::BeginTabItem(LE_ICON_LIST_BOX " Detailed Stats"))
                {
                    ImGui::BeginChild("DetailedStats", ImVec2(0, -40), false);
                    {
                        if (ImGui::BeginTable("MemoryStatsDetailed", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                        {
                            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Current Value", ImGuiTableColumnFlags_WidthFixed, 150);
                            ImGui::TableSetupColumn("Peak Value", ImGuiTableColumnFlags_WidthFixed, 150);
                            ImGui::TableHeadersRow();
                            
                            auto DetailRow = [](const char* label, size_t current, size_t peak = 0, bool showPeak = true)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%s", label);
                                
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%s", ImGuiX::FormatSize(current).c_str());
                                
                                ImGui::TableSetColumnIndex(2);
                                if (showPeak)
                                {
                                    ImGui::Text("%s", ImGuiX::FormatSize(peak).c_str());
                                    if (peak > 0)
                                    {
                                        float usage = (float)current / peak * 100.0f;
                                        ImGui::SameLine();
                                        ImGui::TextColored(
                                            usage > 90 ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(0.5f, 0.8f, 0.5f, 1),
                                            "(%.1f%%)", usage
                                        );
                                    }
                                }
                                else
                                {
                                    ImGui::TextDisabled("-");
                                }
                            };
                            
                            DetailRow("Process Memory", Platform::GetProcessMemoryUsageBytes(), 0, false);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Separator();
                            
                            DetailRow("Current Mapped", Memory::GetCurrentMappedMemory(), Memory::GetPeakMappedMemory());
                            DetailRow("Cached (Small/Medium)", Memory::GetCachedMemory(), 0, false);
                            DetailRow("Current Huge Allocs", Memory::GetCurrentHugeAllocMemory(), Memory::GetPeakHugeAllocMemory());
                            
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Separator();
                            
                            DetailRow("Total Mapped", Memory::GetTotalMappedMemory(), 0, false);
                            DetailRow("Total Unmapped", Memory::GetTotalUnmappedMemory(), 0, false);
                            
                            ImGui::EndTable();
                        }
                        
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                        
                        // Memory efficiency metrics
                        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), LE_ICON_CHART_BAR " Memory Efficiency");
                        ImGui::Spacing();
                        
                        size_t totalMapped = Memory::GetTotalMappedMemory();
                        size_t totalUnmapped = Memory::GetTotalUnmappedMemory();
                        size_t totalOps = totalMapped + totalUnmapped;
                        
                        if (totalOps > 0)
                        {
                            float mappedRatio = (float)totalMapped / totalOps;
                            float unmappedRatio = (float)totalUnmapped / totalOps;
                            
                            ImGui::Text("Allocation Efficiency:");
                            ImGui::SameLine(200);
                            ImGui::ProgressBar(mappedRatio, ImVec2(300, 0), FString().sprintf("Mapped: %.1f%%", mappedRatio * 100).c_str());
                            
                            ImGui::Text("Deallocation Activity:");
                            ImGui::SameLine(200);
                            ImGui::ProgressBar(unmappedRatio, ImVec2(300, 0), FString().sprintf("Unmapped: %.1f%%", unmappedRatio * 100).c_str());
                        }
                        
                        ImGui::Spacing();
                        ImGui::TextDisabled("Note: All values reported by rpmalloc_global_statistics");
                    }
                    ImGui::EndChild();
                    
                    ImGui::EndTabItem();
                }
                
                // Memory Distribution Tab
                if (ImGui::BeginTabItem(LE_ICON_CHART_PIE " Distribution"))
                {
                    size_t processMemory = Platform::GetProcessMemoryUsageBytes();
                    size_t currentMapped = Memory::GetCurrentMappedMemory();
                    size_t cachedMemory = Memory::GetCachedMemory();
                    size_t hugeAllocs = Memory::GetCurrentHugeAllocMemory();
                    
                    // Pie chart
                    if (ImPlot::BeginPlot("Memory Distribution", ImVec2(-1, 350), ImPlotFlags_Equal))
                    {
                        const char* labels[] = { "Mapped", "Cached", "Huge Allocs", "Other" };
                        size_t other = processMemory > (currentMapped + cachedMemory + hugeAllocs) ?
                            processMemory - (currentMapped + cachedMemory + hugeAllocs) : 0;
                        double data[] = { 
                            (double)currentMapped, 
                            (double)cachedMemory, 
                            (double)hugeAllocs,
                            (double)other
                        };
                        
                        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                        ImPlot::SetupAxesLimits(-1, 1, -1, 1);
                        
                        ImPlot::PlotPieChart(labels, data, 4, 0.5, 0.5, 0.4, "%.1f%%", 90);
                        
                        ImPlot::EndPlot();
                    }
                    
                    ImGui::Spacing();
                    
                    // Bar chart comparison
                    if (ImPlot::BeginPlot("Memory Comparison", ImVec2(-1, 250)))
                    {
                        const char* categories[] = { "Mapped", "Peak Mapped", "Cached", "Huge", "Peak Huge" };
                        double values[] = {
                            (double)currentMapped,
                            (double)Memory::GetPeakMappedMemory(),
                            (double)cachedMemory,
                            (double)hugeAllocs,
                            (double)Memory::GetPeakHugeAllocMemory()
                        };
                        
                        ImPlot::SetupAxes("Category", "Bytes", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                        
                        ImPlot::PlotBars("Memory", values, 5, 0.67, 0);
                        
                        ImPlot::EndPlot();
                    }
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // Footer buttons
            if (ImGui::Button("Close", ImVec2(120, 0)))
            {
                return true;
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Export Data", ImVec2(120, 0)))
            {
                // Export history to CSV or similar
            }
            
            return false;
        }, false);
    }

    void FEditorUI::HandleUserInput(const FUpdateContext& UpdateContext)
    {
        
    }

    void FEditorUI::VerifyDirtyPackages()
    {
        TVector<CPackage*> DirtyPackages;
        DirtyPackages.reserve(4);
        for (TObjectIterator<CPackage> Itr; Itr; ++Itr)
        {
            CPackage* Package = *Itr;

            if (Package->IsDirty())
            {
                DirtyPackages.push_back(Package);
            }
        }

        if (DirtyPackages.empty())
        {
            return;
        }
        
        TVector<bool> PackageSelection;
        PackageSelection.resize(DirtyPackages.size(), true);
        
        enum class ESaveState { Idle, Saving, Success, Failed };
        TVector<ESaveState> SaveStates;
        SaveStates.resize(DirtyPackages.size(), ESaveState::Idle);
        
        ModalManager.CreateDialogue("Save Modified Packages", ImVec2(450, 600), [&, Packages = Move(DirtyPackages), PackageSelection, SaveStates](const FUpdateContext&) mutable
        {
            bool bShouldClose = false;
            
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));
            
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), LE_ICON_EXCLAMATION_THICK " Unsaved Changes Detected");
            
            ImGui::Spacing();
            ImGui::TextWrapped("The following packages have unsaved changes. Select which packages you would like to save before continuing.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::BeginGroup();
            {
                if (ImGui::Button(LE_ICON_SELECT_ALL " Select All", ImVec2(140, 0)))
                {
                    for (bool& Selected : PackageSelection)
                    {
                        Selected = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(LE_ICON_SQUARE_OUTLINE " Deselect All", ImVec2(140, 0)))
                {
                    for (bool& Selected : PackageSelection)
                    {
                        Selected = false;
                    }
                }
                
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                
                int32 SelectedCount = 0;
                for (bool Selected : PackageSelection)
                {
                    if (Selected)
                    {
                        SelectedCount++;
                    }
                }

                ImGui::Text("%d of %d selected", SelectedCount, (int32)Packages.size());
            }
            ImGui::EndGroup();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImVec2 ListSize = ImVec2(-1, -80);
            if (ImGui::BeginChild("PackageList", ListSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
                
                for (size_t i = 0; i < Packages.size(); ++i)
                {
                    CPackage* Package = Packages[i];
                    
                    ImGui::PushID((int)i);
                    
                    // Create a subtle background for each item
                    ImVec2 ItemStart = ImGui::GetCursorScreenPos();
                    ImVec2 ItemSize = ImVec2(ImGui::GetContentRegionAvail().x, 64);
                    
                    bool bIsHovered = ImGui::IsMouseHoveringRect(ItemStart, ImVec2(ItemStart.x + ItemSize.x, ItemStart.y + ItemSize.y));
                    
                    ImU32 BgColor = bIsHovered ? 
                        IM_COL32(50, 50, 55, 180) : 
                        IM_COL32(35, 35, 40, 180);
                    
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ItemStart, 
                        ImVec2(ItemStart.x + ItemSize.x, ItemStart.y + ItemSize.y),
                        BgColor,
                        4.0f
                    );
                    
                    ImGui::BeginGroup();
                    {
                        ImGui::Dummy(ImVec2(0, 4));
                        
                        ImGui::Checkbox("##select", &PackageSelection[i]);
                        ImGui::SameLine();
                        
                        ImGui::BeginGroup();
                        {
                            ImGui::Text(LE_ICON_FILE " %s", Package->GetName().c_str());
                            
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", Package->GetFullPackageFilePath().c_str());
                            
                            // Show save state
                            switch (SaveStates[i])
                            {
                                case ESaveState::Saving:
                                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), LE_ICON_WATCH_VIBRATE " Saving...");
                                    break;
                                case ESaveState::Success:
                                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), LE_ICON_CHECK " Saved");
                                    break;
                                case ESaveState::Failed:
                                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), LE_ICON_EXCLAMATION_THICK " Failed to save");
                                    break;
                            }
                        }
                        ImGui::EndGroup();
                        
                        ImGui::Dummy(ImVec2(0, 4));
                    }
                    ImGui::EndGroup();
                    
                    ImGui::PopID();
                    
                    ImGui::Spacing();
                }
                
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::BeginGroup();
            {
                float ButtonWidth = 150.0f;
                float Spacing = 8.0f;
                float TotalWidth = (ButtonWidth * 2) + (Spacing * 2);
                float OffsetX = (ImGui::GetContentRegionAvail().x - TotalWidth) * 0.5f;
                
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + OffsetX);
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
                
                if (ImGui::Button(LE_ICON_CONTENT_SAVE " Save Selected", ImVec2(ButtonWidth, 35)))
                {
                    for (size_t i = 0; i < Packages.size(); ++i)
                    {
                        if (PackageSelection[i])
                        {
                            SaveStates[i] = ESaveState::Saving;
                            
                            bool bSaveSuccess = CPackage::SavePackage(Packages[i], Packages[i]->GetFullPackageFilePath());
                            SaveStates[i] = bSaveSuccess ? ESaveState::Success : ESaveState::Failed;
                        }
                    }
                    
                    bShouldClose = true;
                }
                ImGui::PopStyleColor(3);
                
                ImGui::SameLine();
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.3f, 0.15f, 1.0f));
                
                if (ImGui::Button(LE_ICON_SQUARE " Don't Save", ImVec2(ButtonWidth, 35)))
                {
                    bShouldClose = true;
                }
                ImGui::PopStyleColor(3);
                
                //ImGui::SameLine();
                //
                //if (ImGui::Button(LE_ICON_CANCEL " Cancel", ImVec2(ButtonWidth, 35)))
                //{
                //    
                //    bShouldClose = true;
                //}
            }
            ImGui::EndGroup();
            
            ImGui::PopStyleVar();
            
            return bShouldClose;
        });
    }
    
    void FEditorUI::DrawTitleBarMenu(const FUpdateContext& UpdateContext)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        ImGui::Image(ImGuiX::ToImTextureRef(Paths::GetEngineResourceDirectory() + "/Textures/Lumina.png"), ImVec2(24.0f, 24.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
    
        // Styled menu bar
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.1f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.35f, 0.45f, 0.8f));
    
        ImGui::SetNextWindowSizeConstraints(ImVec2(220, 1), ImVec2(280, 1000));
    
        DrawFileMenu();
        DrawProjectMenu();
        DrawToolsMenu();
        DrawHelpMenu();
    
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);

    }
    
    void FEditorUI::DrawTitleBarInfoStats(const FUpdateContext& UpdateContext)
    {
        ImGui::SameLine();

        // Exponential moving average: smoothed = lerp(smoothed, current, factor)
        float CurrentFPS = UpdateContext.GetFPS();
        float CurrentFrameTime = UpdateContext.GetDeltaTime() * 1000.0f;
        
        SmoothedFPS = SmoothedFPS + (CurrentFPS - SmoothedFPS) * FPSSmoothingFactor;
        SmoothedFrameTime = SmoothedFrameTime + (CurrentFrameTime - SmoothedFrameTime) * FPSSmoothingFactor;

        const TInlineString<100> PerfStats(TInlineString<100>::CtorSprintf(), "FPS: %3.0f / %.2f ms", SmoothedFPS, SmoothedFrameTime);
        ImGui::TextUnformatted(PerfStats.c_str());

        ImGui::SameLine();

        const TInlineString<100> ObjectStats(TInlineString<100>::CtorSprintf(), "CObjects: %i", GObjectArray.GetNumAliveObjects());
        ImGui::TextUnformatted(ObjectStats.c_str());
    }

    void FEditorUI::DrawFileMenu()
    {
        if (!ImGui::BeginMenu(LE_ICON_FILE " File"))
        {
            return;
        }
        // Save Section
        if (ImGui::MenuItem(LE_ICON_ZIP_DISK " Save", "Ctrl+S"))
        {
            // Save action
        }

        if (ImGui::MenuItem(LE_ICON_ZIP_DISK " Save All", "Ctrl+Shift+S"))
        {
            // Save all action
        }

        ImGui::Separator();

        // Recent Files
        if (ImGui::BeginMenu(LE_ICON_ROTATE_LEFT " Recent"))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.62f, 1.0f));
            ImGui::MenuItem("Project1.lumina");
            ImGui::MenuItem("Project2.lumina");
            ImGui::MenuItem("Project3.lumina");
            ImGui::PopStyleColor();

            ImGui::Separator();

            if (ImGui::MenuItem(LE_ICON_TRASH_CAN " Clear Recent"))
            {
                // Clear recent files
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Shaders Submenu
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.6f, 0.4f, 1.0f));
        if (ImGui::BeginMenu(LE_ICON_HAMMER " Shaders"))
        {
            if (ImGui::MenuItem(LE_ICON_HAMMER " Recompile All", "F5"))
            {
                GRenderContext->CompileEngineShaders();
            }

            if (ImGui::MenuItem(LE_ICON_MATERIAL_DESIGN " Recompile Default Material"))
            {
                CMaterial::CreateDefaultMaterial();
            }

            if (ImGui::MenuItem(LE_ICON_FOLDER " Open Shaders Directory", "F6"))
            {
                Platform::LaunchURL(StringUtils::ToWideString(Paths::GetEngineShadersDirectory()).c_str());
            }

            ImGui::Separator();

            for (auto& Directory : std::filesystem::recursive_directory_iterator(Paths::GetEngineShadersDirectory().c_str()))
            {
                if (Directory.is_regular_file())
                {
                    FString FileName = Directory.path().filename().string().c_str();
                    if (ImGui::BeginMenu(FileName.c_str()))
                    {
                        if (ImGui::MenuItem(LE_ICON_HAMMER " Recompile"))
                        {
                            GRenderContext->GetShaderCompiler()->CompileShaderPath(Directory.path().string().c_str(), {}, [&](const FShaderHeader& Header)
                            {
                                GRenderContext->GetShaderLibrary()->CreateAndAddShader(Header.DebugName, Header, true);
                            });
                        }

                        if (ImGui::MenuItem(LE_ICON_FOLDER " Open"))
                        {
                            Platform::LaunchURL(Directory.path().c_str());
                        }

                        ImGui::EndMenu();
                    }
                }
            }

            ImGui::EndMenu();
        }
        ImGui::PopStyleColor();

        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        if (ImGui::MenuItem(LE_ICON_DOOR_OPEN " Exit", "Alt+F4"))
        {
            // ...
			FApplication::RequestExit();
        }
        ImGui::PopStyleColor();

        ImGui::EndMenu();
    }

    void FEditorUI::DrawProjectMenu()
    {
        if (!ImGui::BeginMenu(LE_ICON_FOLDER " Project"))
        {
            return;
        }

        // Project Actions
        if (ImGui::MenuItem(LE_ICON_FOLDER_OPEN " Open Project...", "Ctrl+O"))
        {
            OpenProjectDialog();
        }
    
        if (ImGui::MenuItem(LE_ICON_FOLDER_PLUS " New Project...", "Ctrl+N"))
        {
            NewProjectDialog();
        }
    
        ImGui::Separator();
    
        // Project Settings
        if (ImGui::MenuItem(LE_ICON_SETTINGS_HELPER " Project Settings"))
        {
            ProjectSettingsDialog();
        }
    
        if (ImGui::MenuItem(LE_ICON_DATABASE " Asset Registry"))
        {
            AssetRegistryDialog();
        }
    
        ImGui::Separator();
    
        // Build Options
        if (ImGui::BeginMenu(LE_ICON_HAMMER " Build"))
        {
            if (ImGui::MenuItem(LE_ICON_PLAY " Build Project", "Ctrl+B"))
            {
                // Build project
            }
        
            if (ImGui::MenuItem(LE_ICON_BROOM " Clean Build", "Ctrl+Shift+B"))
            {
                // Clean build
            }
        
            ImGui::Separator();
        
            if (ImGui::MenuItem(LE_ICON_BOX " Package for Windows"))
            {
                // Package
            }
        
            ImGui::EndMenu();
        }
    
        ImGui::EndMenu();
    }

    void FEditorUI::DrawToolsMenu()
    {
        if (!ImGui::BeginMenu(LE_ICON_WRENCH " Tools"))
        {
            return;
        }

        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.62f, 1.0f), "Debug Windows");
        ImGui::Separator();
        
        ImGui::MenuItem(LE_ICON_CHART_LINE " Renderer Info", nullptr, &bShowRenderDebug);
        
        if (ImGui::MenuItem(LE_ICON_MEMORY " Memory Info", nullptr, nullptr))
        {
            DrawMemoryDialog();
        }
        
        ImGui::MenuItem(LE_ICON_LIST_BOX " CObject List", nullptr, &bShowObjectDebug);
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.62f, 1.0f), "ImGui Tools");
        ImGui::Separator();
        
        ImGui::MenuItem(LE_ICON_WINDOW_OPEN " ImGui Demo", nullptr, &bShowDearImGuiDemoWindow);
        ImGui::MenuItem(LE_ICON_CHART_BAR " ImPlot Demo", nullptr, &bShowImPlotDemoWindow);
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.62f, 1.0f), "External Tools");
        ImGui::Separator();
        
        if (ImGui::MenuItem(LE_ICON_WATCH" Tracy Profiler", "Ctrl+P"))
        {
            LaunchTracyProfiler();
        }
        
        if (ImGui::MenuItem(LE_ICON_CAMERA " RenderDoc Capture", "F11"))
        {
            FRenderDoc::Get().TriggerCapture();
        }
        
        ImGui::Spacing();
        
        // Settings
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.62f, 1.0f), "Settings");
        ImGui::Separator();
        
        bool bVSyncEnabled = GRenderContext->IsVSyncEnabled();
        if (ImGui::MenuItem(LE_ICON_DISC_PLAYER " V-Sync", nullptr, bVSyncEnabled))
        {
            GRenderContext->SetVSyncEnabled(!bVSyncEnabled);
        }
        
        if (ImGui::BeginMenu(LE_ICON_PALETTE " Theme"))
        {
            if (ImGui::MenuItem("Dark", nullptr, true))  // Currently selected
            {
                // Apply dark theme
            }
            
            if (ImGui::MenuItem("Light", nullptr, false))
            {
                // Apply light theme
            }
            
            if (ImGui::MenuItem("Custom...", nullptr, false))
            {
                // Open theme editor
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMenu();
    }

    void FEditorUI::DrawHelpMenu()
    {
        if (!ImGui::BeginMenu(LE_ICON_HELP " Help"))
        {
            return;
        }

        if (ImGui::MenuItem(LE_ICON_GROUP " Discord"))
        {
            Platform::LaunchURL(TEXT("https://discord.gg/UhTmzB8UdY"));
        }

        if (ImGui::MenuItem(LE_ICON_BOOK " Documentation", "F1"))
        {
            Platform::LaunchURL(TEXT("https://discord.gg/UhTmzB8UdY"));
        }
    
        if (ImGui::MenuItem(LE_ICON_ACCOUNT_QUESTION " Tutorials"))
        {
            Platform::LaunchURL(TEXT("https://discord.gg/UhTmzB8UdY"));
        }
    
        ImGui::Separator();
    
        if (ImGui::MenuItem(LE_ICON_GITHUB " GitHub Repository"))
        {
            Platform::LaunchURL(TEXT("https://github.com/MrDrElliot/LuminaEngine"));
        }
    
        if (ImGui::MenuItem(LE_ICON_BUG " Report Issue"))
        {
            Platform::LaunchURL(TEXT("https://github.com/MrDrElliot/LuminaEngine/issues"));
        }
    
        ImGui::Separator();
    
        if (ImGui::MenuItem(LE_ICON_CIRCLE " About Lumina"))
        {
            bShowLuminaInfo = !bShowLuminaInfo;
        }
    
        ImGui::EndMenu();
    }

    void FEditorUI::OpenProjectDialog()
    {
        ModalManager.CreateDialogue("Open Project", ImVec2(1000, 650), [this](const FUpdateContext& Ctx) -> bool
        {
            bool bShouldClose = false;

            // Header section
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::TextWrapped(LE_ICON_FOLDER_OPEN " Select a project to open or browse for an existing project");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Main content area
            ImGui::BeginChild("ProjectContent", ImVec2(0, -50), false);
            {
                // Recent/Example Projects Section
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
                ImGui::Text(LE_ICON_FOLDER_OPEN " Example Projects");
                ImGui::PopStyleColor();
                ImGui::Spacing();

                // Project cards container
                ImGui::BeginChild("ProjectCards", ImVec2(0, 0), false);
                {
                    const float cardWidth = 280.0f;
                    const float cardHeight = 200.0f;
                    const float padding = 16.0f;

                    // Calculate how many cards fit per row
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    int cardsPerRow = Math::Max(1, (int)((availWidth + padding) / (cardWidth + padding)));

                    // Sandbox Project Card
                    ImGui::BeginGroup();
                    {
                        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                        ImDrawList* drawList = ImGui::GetWindowDrawList();

                        // Card background
                        ImVec4 cardBgColor = ImVec4(0.15f, 0.15f, 0.16f, 1.0f);
                        ImVec4 cardBgHoverColor = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
                        ImVec4 accentColor = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);

                        ImGui::PushStyleColor(ImGuiCol_Button, cardBgColor);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cardBgHoverColor);
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.21f, 1.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 12));

                        if (ImGui::Button("##SandboxCard", ImVec2(cardWidth, cardHeight)))
                        {
                            FString SandboxProjectDirectory = Paths::GetEngineInstallDirectory() + "/Applications/Sandbox/Sandbox.lproject";
                            GEditorEngine->GetProject().LoadProject(SandboxProjectDirectory);
                            ContentBrowser->RefreshContentBrowser();
                            GEngine->GetEngineSubsystem<FAssetRegistry>()->RunInitialDiscovery();
                            bShouldClose = true;
                        }

                        ImGui::PopStyleVar(2);
                        ImGui::PopStyleColor(3);

                        // Draw accent line at top of card
                        drawList->AddRectFilled(
                            cursorPos,
                            ImVec2(cursorPos.x + cardWidth, cursorPos.y + 4),
                            ImGui::GetColorU32(accentColor)
                        );

                        // Card content overlay
                        ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 16, cursorPos.y + 20));
                        ImGui::BeginGroup();
                        {
                            // Icon circle background
                            ImVec2 iconPos = ImGui::GetCursorScreenPos();
                            drawList->AddCircleFilled(
                                ImVec2(iconPos.x + 20, iconPos.y + 20),
                                20.0f,
                                ImGui::GetColorU32(ImVec4(0.3f, 0.6f, 1.0f, 0.2f))
                            );

                            ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
                            ImGui::SetCursorScreenPos(ImVec2(iconPos.x + 10, iconPos.y + 10));
                            ImGui::Text(LE_ICON_FOLDER_OPEN);
                            ImGui::PopStyleColor();

                            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 16, iconPos.y + 50));

                            // Project name
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                            ImGui::Text("Sandbox Project");
                            ImGui::PopStyleColor();

                            ImGui::Spacing();

                            // Description
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                            ImGui::BeginChild("##SandboxDesc", ImVec2(cardWidth - 32, 60), false, ImGuiWindowFlags_NoScrollbar);
                            ImGui::TextWrapped("A basic sandbox environment for testing and experimentation. Perfect for learning the engine basics.");
                            ImGui::EndChild();
                            ImGui::PopStyleColor();

                            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 16, cursorPos.y + cardHeight - 30));

                            // Project type badge
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
                            ImGui::Text("Example Project");
                            ImGui::PopStyleColor();

                            ImGui::EndGroup();
                        }
                    }
                    ImGui::EndGroup();


                    ImGui::EndChild();
                }

                ImGui::EndChild();
            }

            // Bottom action bar
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginGroup();
            {
                // Browse button (left side)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));

                if (ImGui::Button(LE_ICON_FOLDER_OPEN " Browse for Project...", ImVec2(200, 32)))
                {
                    FString Project;
                    if (Platform::OpenFileDialogue(Project, "Open Project", "Lumina Project (*.lproject)\0*.lproject\0All Files (*.*)\0*.*\0"))
                    {
                        GEditorEngine->GetProject().LoadProject(Project);
                        ContentBrowser->RefreshContentBrowser();
                        GEngine->GetEngineSubsystem<FAssetRegistry>()->RunInitialDiscovery();
                        bShouldClose = true;
                    }
                }

                ImGui::PopStyleColor(3);

                // Cancel button (right side)
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

                if (ImGui::Button("Cancel", ImVec2(120, 32)))
                {
                    bShouldClose = true;
                }

                ImGui::PopStyleColor(3);

                ImGui::EndGroup();
            }

            return bShouldClose;
        });
    }

    void FEditorUI::NewProjectDialog()
    {
        ModalManager.CreateDialogue("New Project", ImVec2(900, 600), [this](const FUpdateContext& Ctx) -> bool
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LE_ICON_FOLDER_PLUS " Create a new Lumina project");
            ImGui::Separator();
            ImGui::Spacing();
            
            static char ProjectName[256] = "MyProject";
            static char ProjectPath[512] = "";
            
            ImGui::Text("Project Name:");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##ProjectName", ProjectName, sizeof(ProjectName));
            
            ImGui::Spacing();
            
            ImGui::Text("Project Location:");
            ImGui::SetNextItemWidth(-120);
            ImGui::InputText("##ProjectPath", ProjectPath, sizeof(ProjectPath));
            ImGui::SameLine();
            if (ImGui::Button("Browse...", ImVec2(110, 0)))
            {
                // Open folder browser
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // Template selection
            ImGui::Text("Project Template:");
            ImGui::BeginChild("Templates", ImVec2(0, -40), true);
            {
                if (ImGui::Selectable(LE_ICON_CUBE " Blank Project"))
                {
                    // Select blank template
                }
                
                if (ImGui::Selectable(LE_ICON_GAMEPAD " First Person Template"))
                {
                    // Select FPS template
                }
                
                if (ImGui::Selectable(LE_ICON_CAR " Vehicle Template"))
                {
                    // Select vehicle template
                }
            }
            ImGui::EndChild();
            
            ImGui::Spacing();
            
            // Action buttons
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.3f, 1.0f));
            if (ImGui::Button(LE_ICON_CHECK " Create Project", ImVec2(140, 0)))
            {
                // Create project
                return true;
            }
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                return true;
            }
            
            return false;
        });
    }

    void FEditorUI::ProjectSettingsDialog()
    {
        ModalManager.CreateDialogue("Project Settings", ImVec2(1000, 700), [this](const FUpdateContext& Ctx) -> bool
        {
            // Sidebar with categories
            ImGui::BeginChild("SettingsCategories", ImVec2(200, 0), true);
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CATEGORIES");
                ImGui::Separator();
                ImGui::Spacing();
                
                if (ImGui::Selectable(LE_ICON_STAR_SETTINGS " General", true))
                {
                    // Select general
                }
                
                if (ImGui::Selectable(LE_ICON_BRUSH " Graphics"))
                {
                    // Select graphics
                }
                
                if (ImGui::Selectable(LE_ICON_VOLUME_HIGH " Audio"))
                {
                    // Select audio
                }
                
                if (ImGui::Selectable(LE_ICON_KEYBOARD " Input"))
                {
                    // Select input
                }
                
                if (ImGui::Selectable(LE_ICON_PUZZLE " Plugins"))
                {
                    // Select plugins
                }
            }
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            // Settings content
            ImGui::BeginChild("SettingsContent", ImVec2(0, -40), true);
            {
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "General Settings");
                ImGui::Separator();
                ImGui::Spacing();
                
                // Add settings controls here
                ImGui::Text("Project settings content goes here...");
            }
            ImGui::EndChild();
            
            ImGui::Spacing();
            
            // Close button
            if (ImGui::Button("Close", ImVec2(120, 0)))
            {
                return true;
            }
            
            return false;
        }, false);
    }

    void FEditorUI::AssetRegistryDialog()
    {
        ModalManager.CreateDialogue("Asset Registry", ImVec2(1400, 800), [this](const FUpdateContext& Ctx) -> bool
        {
            // Static state variables
            static char searchFilter[256] = "";
            static int sortMode = 0; // 0=Name, 1=Type, 2=Path, 3=References
            static bool sortAscending = true;
            static FAssetData* selectedAsset = nullptr;
            static int viewMode = 0; // 0=List, 1=Grid

            FAssetRegistry* Registry = GEngine->GetEngineSubsystem<FAssetRegistry>();
            const auto& AssetsByPath = Registry->GetAssetsByPath();

            // Collect and filter assets
            struct FAssetDisplay
            {
                FAssetData* Asset;
                FString Path;
                int32 RefCount;
            };

            static TVector<FAssetDisplay> filteredAssets;
            static bool needsRefresh = true;

            if (needsRefresh)
            {
                filteredAssets.clear();
                for (auto& [Path, AssetVec] : AssetsByPath)
                {
                    for (auto* Asset : AssetVec)
                    {
                        FAssetDisplay display;
                        display.Asset = Asset;
                        display.Path = Path;
                        display.RefCount = Registry->GetReferences(Asset->PackageName).size();
                        filteredAssets.push_back(display);
                    }
                }
                needsRefresh = false;
            }

            // Header
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), LE_ICON_DATABASE);
            ImGui::SameLine();
            ImGui::Text("Asset Registry Browser");
            ImGui::PopStyleColor();

            ImGui::SameLine(ImGui::GetWindowWidth() - 180);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%d Assets", (int)filteredAssets.size());

            ImGui::Separator();
            ImGui::Spacing();

            // Toolbar
            ImGui::BeginGroup();
            {
                // Search bar
                ImGui::SetNextItemWidth(300);
                if (ImGui::InputTextWithHint("##search", LE_ICON_BOOK_SEARCH " Search assets...", searchFilter, IM_ARRAYSIZE(searchFilter)))
                {
                    needsRefresh = true;
                }

                ImGui::SameLine();

                // Sort dropdown
                ImGui::SetNextItemWidth(150);
                const char* sortItems[] = { "Name", "Type", "Path", "References" };
                if (ImGui::Combo("##sort", &sortMode, sortItems, IM_ARRAYSIZE(sortItems)))
                {
                    // Sort assets
                    eastl::sort(filteredAssets.begin(), filteredAssets.end(), [](const FAssetDisplay& a, const FAssetDisplay& b)
                    {
                        bool result = false;
                        switch (sortMode)
                        {
                            case 0: result = a.Asset->PackageName.ToString() < b.Asset->PackageName.ToString(); break;
                            case 1: result = a.Asset->AssetClass < b.Asset->AssetClass; break;
                            case 2: result = a.Path < b.Path; break;
                            case 3: result = a.RefCount < b.RefCount; break;
                        }
                        return sortAscending ? result : !result;
                    });
                }

                ImGui::SameLine();
                if (ImGui::Button(sortAscending ? LE_ICON_ARROW_UP : LE_ICON_ARROW_DOWN))
                {
                    sortAscending = !sortAscending;
                    needsRefresh = true;
                }

                ImGui::SameLine();
                ImGui::Separator();
                ImGui::SameLine();

                // View mode toggle
                if (ImGui::Button(viewMode == 0 ? LE_ICON_LIST_BOX : LE_ICON_GRID))
                {
                    viewMode = (viewMode + 1) % 2;
                }

                ImGui::SameLine();
                if (ImGui::Button(LE_ICON_REFRESH " Refresh"))
                {
                    needsRefresh = true;
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();

            // Main content area with splitter
            static float leftPanelWidth = 800.0f;

            ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, -40), true);
            {
                // Column headers
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
                ImGui::Columns(4, "AssetColumns", true);

                if (ImGui::GetColumnWidth(0) > 10) // First time setup
                {
                    ImGui::SetColumnWidth(0, 300);
                    ImGui::SetColumnWidth(1, 200);
                    ImGui::SetColumnWidth(2, 200);
                    ImGui::SetColumnWidth(3, 80);
                }

                ImGui::Separator();
                ImGui::Text("Asset Name"); ImGui::NextColumn();
                ImGui::Text("Type"); ImGui::NextColumn();
                ImGui::Text("Path"); ImGui::NextColumn();
                ImGui::Text("Refs"); ImGui::NextColumn();
                ImGui::Separator();

                // Asset list
                ImGuiListClipper clipper;
                clipper.Begin((int)filteredAssets.size());

                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        FAssetDisplay& assetDisplay = filteredAssets[i];
                        FAssetData* asset = assetDisplay.Asset;

                        // Apply search filter
                        FString assetName = asset->PackageName.ToString();
                        if (searchFilter[0] != '\0' &&
                            assetName.find(searchFilter) == INDEX_NONE &&
                            assetDisplay.Path.find(searchFilter) == INDEX_NONE)
                        {
                            continue;
                        }

                        ImGui::PushID(i);

                        // Selection
                        bool isSelected = (selectedAsset == asset);
                        if (isSelected)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.7f, 0.8f));
                        }

                        // Asset name with icon
                        FString className = asset->AssetClass.ToString();
                        const char* icon = LE_ICON_FILE;
                        if (ImGui::Selectable((FString(icon) + " " + assetName).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            selectedAsset = asset;
                        }

                        if (isSelected)
                        {
                            ImGui::PopStyleColor();
                        }

                        ImGui::NextColumn();

                        // Type with color coding
                        ImVec4 typeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                        ImGui::TextColored(typeColor, "%s", className.c_str());
                        ImGui::NextColumn();

                        // Path
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", assetDisplay.Path.c_str());
                        ImGui::NextColumn();

                        // Reference count with badge
                        if (assetDisplay.RefCount > 0)
                        {
                            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "%d", assetDisplay.RefCount);
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "0");
                        }
                        ImGui::NextColumn();

                        ImGui::PopID();
                    }
                }

                ImGui::Columns(1);
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();

            // Splitter
            ImGui::SameLine();
            ImGui::Button("##splitter", ImVec2(4, -40));
            if (ImGui::IsItemActive())
            {
                leftPanelWidth += ImGui::GetIO().MouseDelta.x;
                leftPanelWidth = ImClamp(leftPanelWidth, 400.0f, 1100.0f);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }

            ImGui::SameLine();

            // Right panel - Asset details
            ImGui::BeginChild("RightPanel", ImVec2(0, -40), true);
            {
                if (selectedAsset)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                    ImGui::Text(LE_ICON_DETAILS " Asset Details");
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Asset information
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Name:");
                    ImGui::SameLine(120);
                    ImGui::Text("%s", selectedAsset->PackageName.ToString().c_str());

                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Type:");
                    ImGui::SameLine(120);
                    ImGui::Text("%s", selectedAsset->AssetClass.c_str());

                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Package:");
                    ImGui::SameLine(120);
                    ImGui::Text("%s", selectedAsset->PackageName.ToString().c_str());

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // References section
                    const THashSet<FName>& references = Registry->GetReferences(selectedAsset->PackageName);

                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), LE_ICON_LINK " References (%d)", references.size());
                    ImGui::Separator();

                    ImGui::BeginChild("References", ImVec2(0, 0), false);
                    {
                        if (!references.empty())
                        {
                            for (const FName& refName : references)
                            {
                                ImGui::BulletText("%s", refName.ToString().c_str());

                                // Context menu for each reference
                                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                                {
                                    ImGui::OpenPopup("RefContext");
                                }
                            }
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No references");
                        }
                    }
                    ImGui::EndChild();
                }
                else
                {
                    // No selection placeholder
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - 40);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

                    float textWidth = ImGui::CalcTextSize("Select an asset to view details").x;
                    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) / 2);
                    ImGui::Text(LE_ICON_INFORMATION);

                    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) / 2);
                    ImGui::Text("Select an asset to view details");

                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Footer buttons
            if (ImGui::Button("Close", ImVec2(120, 0)))
            {
                selectedAsset = nullptr;
                return true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Export List", ImVec2(120, 0)))
            {
                // Export functionality could go here
            }

            return false;
        }, false);
    }

    void FEditorUI::LaunchTracyProfiler()
    {
        FString LuminaDirEnv = std::getenv("LUMINA_DIR");
        FString FullPath = LuminaDirEnv + "/External/Tracy/tracy-profiler.exe";
    
#ifdef _WIN32
        FString FullCommand = "start \"\" \"" + FullPath + "\"";
#else
        FString FullCommand = FullPath + " &";
#endif
    
        std::system(FullCommand.c_str());
    }
}
