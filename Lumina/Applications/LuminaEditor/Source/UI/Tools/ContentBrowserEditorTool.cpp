#include "ContentBrowserEditorTool.h"

#include "EditorToolContext.h"
#include "LuminaEditor.h"
#include "Assets/AssetManager/AssetManager.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Assets/Definition/AssetDefinition.h"
#include "Assets/Factories/Factory.h"
#include "Core/Engine/Engine.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/ObjectRedirector.h"
#include "Core/Object/Package/Package.h"
#include "Core/Object/Package/Thumbnail/PackageThumbnail.h"
#include "Core/Windows/Window.h"
#include "EASTL/sort.h"
#include "Input/Input.h"
#include "Paths/Paths.h"
#include "Platform/Process/PlatformProcess.h"
#include "Project/Project.h"
#include "Renderer/RenderManager.h"
#include "TaskSystem/TaskSystem.h"
#include "thumbnails/thumbnailmanager.h"
#include "Tools/Dialogs/Dialogs.h"
#include "Tools/Import/ImportHelpers.h"
#include "Tools/UI/ImGui/ImGuiMemoryEditor.h"
#include "Tools/UI/ImGui/ImGuiRenderer.h"
#include "Tools/UI/ImGui/ImGuiX.h"
#include "UI/EditorUI.h"

namespace Lumina
{

    class FRenameModalState
    {
    public:
        
        char Buffer[256] = {};
        bool bInitialized = false;
    
        void Initialize(const FString& CurrentName)
        {
            if (!bInitialized)
            {
                strncpy_s(Buffer, sizeof(Buffer), CurrentName.c_str(), _TRUNCATE);
                bInitialized = true;
            }
        }
    
        void Reset()
        {
            memset(Buffer, 0, sizeof(Buffer));
            bInitialized = false;
        }
    };
    
    void FContentBrowserEditorTool::RefreshContentBrowser()
    {
        ContentBrowserTileView.MarkTreeDirty();
        OutlinerListView.MarkTreeDirty();
    }

    void FContentBrowserEditorTool::OnInitialize()
    {
        FWindow::OnWindowDropped.AddMember(this, &FContentBrowserEditorTool::OnWindowDropped);
        
        
        using namespace Import::Textures;

        for (TObjectIterator<CFactory> It; It; ++It)
        {
            CFactory* Factory = *It;
            if (Factory->HasAnyFlag(OF_DefaultObject))
            {
                if (CClass* AssetClass = Factory->GetSupportedType())
                {
                    FilterState.emplace(AssetClass->GetName().c_str(), true);
                }
            }
        }

        FilterState.emplace(CObjectRedirector::StaticName().c_str(), false);

        CreateToolWindow("Content", [this] (const FUpdateContext& Contxt, bool bIsFocused)
        {
            float Left = 200.0f;
            float Right = ImGui::GetContentRegionAvail().x - Left;
            
            DrawDirectoryBrowser(Contxt, bIsFocused, ImVec2(Left, 0));
            
            ImGui::SameLine();

            DrawContentBrowser(Contxt, bIsFocused, ImVec2(Right, 0));
        });
        
        SelectedPath = GEditorEngine->GetProject().GetProjectContentDirectory().c_str();

        GEngine->GetEngineSubsystem<FAssetRegistry>()->GetOnAssetRegistryUpdated().AddMember(this, &FContentBrowserEditorTool::RefreshContentBrowser);
        
        ContentBrowserTileViewContext.DragDropFunction = [this] (FTileViewItem* DropItem)
        {
            auto* TypedDroppedItem = (FContentBrowserTileViewItem*)DropItem;
            if (!TypedDroppedItem->IsDirectory())
            {
                return;
            }
            
            const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(FContentBrowserTileViewItem::DragDropID, ImGuiDragDropFlags_AcceptBeforeDelivery);
            if (Payload && Payload->IsDelivery())
            {
                uintptr_t ValuePtr = *static_cast<uintptr_t*>(Payload->Data);
                auto* SourceItem = reinterpret_cast<FContentBrowserTileViewItem*>(ValuePtr);

                if (SourceItem == TypedDroppedItem)
                {
                    return;
                }

                HandleContentBrowserDragDrop(TypedDroppedItem, SourceItem);
                
            }
        };

ContentBrowserTileViewContext.DrawItemOverrideFunction = [this] (FTileViewItem* Item) -> bool
{
    FContentBrowserTileViewItem* ContentItem = static_cast<FContentBrowserTileViewItem*>(Item);
    
    ImTextureRef ImTexture = ImGuiX::ToImTextureRef(Paths::GetEngineResourceDirectory() + "/Textures/Folder.png");

    ImVec4 TintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    if (!ContentItem->IsDirectory())
    {
        if (ContentItem->GetAssetData().IsRedirector())
        {
            ImTexture = ImGuiX::ToImTextureRef(Paths::GetEngineResourceDirectory() + "/Textures/Redirect.png");
            TintColor = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
        }
        else
        {
            if (CPackage* Package = FindObject<CPackage>(nullptr, ContentItem->GetAssetData().PackageName))
            {
                if (Package->GetPackageThumbnail()->LoadedImage)
                {
                    ImTexture = GEngine->GetEngineSubsystem<FRenderManager>()->GetImGuiRenderer()->GetOrCreateImTexture(Package->GetPackageThumbnail()->LoadedImage);
                }
                else
                {
                    ImTexture = ImGuiX::ToImTextureRef(Paths::GetEngineResourceDirectory() + "/Textures/SkeletalMeshIcon.png");
                }
            }
            else
            {
                ImTexture = ImGuiX::ToImTextureRef(Paths::GetEngineResourceDirectory() + "/Textures/SkeletalMeshIcon.png");
            }
        }
    }
    else
    {
        TintColor = ImVec4(1.0f, 0.9f, 0.6f, 1.0f);
    }
    
    // Minimal padding for larger image display
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.17f, 1.0f)); 
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 Pos = ImGui::GetCursorScreenPos();
    ImVec2 Size = ImVec2(120.0f, 120.0f);
    
    // Draw drop shadow
    DrawList->AddRectFilled(
        ImVec2(Pos.x + 3, Pos.y + 3),
        ImVec2(Pos.x + Size.x + 11, Pos.y + Size.y + 11),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)),
        8.0f
    );
    
    bool clicked = ImGui::ImageButton("##", ImTexture, Size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TintColor);

    // Hover highlight
    if (ImGui::IsItemHovered())
    {
        DrawList->AddRect(
            Pos, 
            ImVec2(Pos.x + Size.x + 8, Pos.y + Size.y + 8), 
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.6f, 0.9f, 0.7f)), 
            8.0f, 
            0, 
            2.0f
        );
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        return true;
    }

    return clicked;
};
        
        ContentBrowserTileViewContext.ItemSelectedFunction = [this] (FTileViewItem* Item)
        {
            FContentBrowserTileViewItem* ContentItem = static_cast<FContentBrowserTileViewItem*>(Item);
            if (ContentItem->IsDirectory())
            {
                SelectedPath = ContentItem->GetPath();
                RefreshContentBrowser();
            }
            else
            {
                FString QualifiedName = ContentItem->GetAssetData().FullPath.ToString();

                CObject* Asset = LoadObject<CObject>(nullptr, QualifiedName);
                ToolContext->OpenAssetEditor(Asset);
            }
        };
        
        ContentBrowserTileViewContext.DrawItemContextMenuFunction = [this] (const TVector<FTileViewItem*>& Items)
        {
            bool bMultipleItems = Items.size() > 1;
            bool bAllDirectories = true;
            bool bAllAssets = true;
            bool bHasRedirectors = false;
            
            // Analyze selection
            for (FTileViewItem* Item : Items)
            {
                FContentBrowserTileViewItem* ContentItem = static_cast<FContentBrowserTileViewItem*>(Item);
                if (ContentItem->IsDirectory())
                {
                    bAllAssets = false;
                }
                else
                {
                    bAllDirectories = false;
                    if (ContentItem->GetAssetData().IsRedirector())
                    {
                        bHasRedirectors = true;
                    }
                }
            }
            
            for (FTileViewItem* Item : Items)
            {
                FContentBrowserTileViewItem* ContentItem = static_cast<FContentBrowserTileViewItem*>(Item);
                
                if (!bMultipleItems)
                {
                    if (!ContentItem->IsDirectory() && !ContentItem->GetAssetData().IsRedirector())
                    {
                        if (ImGui::MenuItem(LE_ICON_FOLDER_OPEN " Open", "Double-Click"))
                        {
                            FString QualifiedName = ContentItem->GetAssetData().FullPath.ToString();
                            CObject* Asset = LoadObject<CObject>(nullptr, QualifiedName);
                            ToolContext->OpenAssetEditor(Asset);
                        }
                        ImGui::Separator();
                    }
                    
                    if (ContentItem->GetAssetData().IsRedirector())
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 100, 255));
                        if (ImGui::MenuItem(LE_ICON_TOOLBOX " Fix Redirector"))
                        {
                            // Fixup redirector logic
                        }
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                    }
                }
                
                if (!bMultipleItems)
                {
                    if (ImGui::MenuItem(LE_ICON_ARCHIVE_EDIT " Rename", "F2"))
                    {
                        auto RenameState = MakeSharedPtr<FRenameModalState>();
                
                        ToolContext->PushModal("Rename Asset", ImVec2(400.0f, 180.0f), [this, ContentItem, RenameState](const FUpdateContext&) -> bool
                        {
                            RenameState->Initialize(ContentItem->GetFileName());
        
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Enter new name:");
                            ImGui::Spacing();
                            
                            ImGui::SetKeyboardFocusHere();
                            ImGui::SetNextItemWidth(-1);
                            
                            bool bSubmitted = ImGui::InputText("##RenameInput", RenameState->Buffer, sizeof(RenameState->Buffer), 
                                ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue);
        
                            ImGui::Spacing();
                            
                            // Validation feedback
                            if (!ContentItem->IsDirectory() && strlen(RenameState->Buffer) > 0)
                            {
                                FString TestAssetName = ContentItem->GetAssetData().PackageName.ToString() + "." + RenameState->Buffer;
                                if (CObject* TestObject = FindObject<CObject>(nullptr, TestAssetName))
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
                                    ImGui::TextWrapped(LE_ICON_ALERT_OCTAGON " Asset already exists: %s", *TestObject->GetQualifiedName());
                                    ImGui::PopStyleColor();
                                    
                                    ImGui::Spacing();
                                    if (ImGui::Button("Cancel", ImVec2(-1, 0)))
                                    {
                                        return true;
                                    }
                                    return false;
                                }
                            }
                            
                            if (bSubmitted && strlen(RenameState->Buffer) > 0)
                            {
                                FString NewPath = Paths::Parent(ContentItem->GetPath()) + "/" + RenameState->Buffer;

                                PendingRenames.emplace_back(FPendingRename{ContentItem->GetPath(), NewPath});
                                RenameState->Reset();
                                return true;
                            }
        
                            // Action buttons
                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::Spacing();
                            
                            float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                            
                            bool bCanRename = strlen(RenameState->Buffer) > 0;
                            if (!bCanRename)
                            {
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                            }
                            
                            if (ImGui::Button("Rename", ImVec2(buttonWidth, 0)))
                            {
                                if (bCanRename)
                                {
                                    FString NewPath = Paths::Parent(ContentItem->GetPath()) + "/" + RenameState->Buffer;
                                    PendingRenames.emplace_back(FPendingRename{ContentItem->GetPath(), NewPath});
                                    RenameState->Reset();
                                    return true;
                                }
                            }
                            
                            if (!bCanRename)
                            {
                                ImGui::PopItemFlag();
                                ImGui::PopStyleVar();
                            }
                            
                            ImGui::SameLine();
                            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                            {
                                return true;
                            }

                            return false;
                        });
                    }
                    
                    ImGui::Separator();
                }
                
                if (ImGui::MenuItem(LE_ICON_FOLDER " Show in Explorer", nullptr, false, !bMultipleItems))
                {
                    FString PackagePath = ContentItem->GetPath();
                    PackagePath = Paths::Parent(PackagePath);
                    Platform::LaunchURL(UTF8_TO_WIDE(PackagePath).c_str());
                }
                
                if (ImGui::MenuItem(LE_ICON_CONTENT_COPY " Copy Path", nullptr, false, !bMultipleItems))
                {
                    ImGui::SetClipboardText(ContentItem->GetPath().c_str());
                    ImGuiX::Notifications::NotifyInfo("Path copied to clipboard");
                }
                
                ImGui::Separator();
                
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
                bool bDeleteClicked = ImGui::MenuItem(LE_ICON_ALERT_OCTAGON " Delete", "Del");
                ImGui::PopStyleColor();
                
                if (bDeleteClicked)
                {
                    FString ConfirmMessage = bMultipleItems ?
                        std::format("Are you sure you want to delete {0} items?", Items.size()).c_str() :
                        std::format("Are you sure you want to delete \"{0}\"?\n\nThis action cannot be undone.", ContentItem->GetFileName().c_str()).c_str();
                    
                    if (Dialogs::Confirmation("Confirm Deletion", ConfirmMessage.c_str()))
                    {
                        try
                        {
                            FString PackagePath = ContentItem->GetPath();
                            bool bWasSuccessful = false;
        
                            if (std::filesystem::is_directory(PackagePath.c_str()))
                            {
                                if (std::filesystem::is_empty(PackagePath.c_str()))
                                {
                                    std::filesystem::remove(PackagePath.c_str());
                                    bWasSuccessful = true;
                                    RefreshContentBrowser();   
                                }
                            }
                            else
                            {
                                FString ObjectName = ContentItem->GetFileName();
                                FString QualifiedName = ContentItem->GetVirtualPath() + "." + ObjectName;
        
                                if (CObject* AliveObject = FindObject<CObject>(nullptr, QualifiedName))
                                {
                                    ToolContext->OnDestroyAsset(AliveObject);
                                    bWasSuccessful = true;
                                }

                                PendingDestroy.emplace_back(FPendingDestroy{PackagePath});
                            }
        
                            if (bWasSuccessful)
                            {
                                ImGuiX::Notifications::NotifySuccess("Deleted \"%s\"", ContentItem->GetFileName().c_str());
                            }
                            else
                            {
                                ImGuiX::Notifications::NotifyError("Failed to delete \"%s\"", ContentItem->GetFileName().c_str());
                            }
                        }
                        catch (const std::filesystem::filesystem_error& e)
                        {
                            LOG_ERROR("Failed to delete file: {0}", e.what());
                            ImGuiX::Notifications::NotifyError("Deletion failed: %s", e.what());
                        }
                    }
                }
            }
        };

        ContentBrowserTileViewContext.RebuildTreeFunction = [this] (FTileViewWidget* Tree)
        {
            if (Paths::Exists(SelectedPath))
            {
                TFixedVector<FString, 24> DirectoryPaths;
                
                for (auto& Directory : std::filesystem::directory_iterator(SelectedPath.c_str()))
                {
                    if (std::filesystem::is_directory(Directory))
                    {
                        FString VirtualPath = Paths::ConvertToVirtualPath(Directory.path().generic_string().c_str());
                        DirectoryPaths.push_back(Directory.path().generic_string().c_str());
                    }
                }

                eastl::sort(DirectoryPaths.begin(), DirectoryPaths.end());
                
                for (const FString& Directory : DirectoryPaths)
                {
                    ContentBrowserTileView.AddItemToTree<FContentBrowserTileViewItem>(nullptr, Directory, nullptr);
                }

                FString FullPath = Paths::ConvertToVirtualPath(SelectedPath);
                TVector<FAssetData*> Assets = GEngine->GetEngineSubsystem<FAssetRegistry>()->GetAssetsForPath(FullPath);
                
                eastl::sort(Assets.begin(), Assets.end(), [](const FAssetData* A, const FAssetData* B)
                {
                    return A->AssetName.ToString() > B->AssetName.ToString();
                });
                
                for (FAssetData* Asset : Assets)
                {
                    FName ShortClassName = Paths::GetExtension(Asset->AssetClass.ToString());
                    if (FilterState.count(ShortClassName) && FilterState.at(ShortClassName))
                    {
                        FString PackageFullPath = Paths::ResolveVirtualPath(Asset->PackageName.ToString());
                        CThumbnailManager::Get().GetOrLoadThumbnailsForPackage(PackageFullPath);
                        FullPath = Paths::ResolveVirtualPath(Asset->FullPath.ToString());
                        ContentBrowserTileView.AddItemToTree<FContentBrowserTileViewItem>(nullptr, std::move(FullPath), Asset);
                    }
                }
            }
        };

        OutlinerContext.DrawItemContextMenuFunction = [this](const TVector<FTreeListViewItem*>& Items)
        {
            for (FTreeListViewItem* Item : Items)
            {
                //FContentBrowserListViewItem* ContentItem = static_cast<FContentBrowserListViewItem*>(Item);
                
            }
        };
        
        OutlinerContext.RebuildTreeFunction = [this](FTreeListView* Tree)
        {
            for (const auto& [VirtualPrefix, PhysicalRootStr] : Paths::GetMountedPaths())
            {
                auto* RootItem = OutlinerListView.AddItemToTree<FContentBrowserListViewItem>(nullptr, PhysicalRootStr, VirtualPrefix.ToString());

                TFunction<void(FContentBrowserListViewItem*, const FString&)> AddChildrenRecursive;

                AddChildrenRecursive = [&](FContentBrowserListViewItem* ParentItem, const FString& CurrentPath)
                {
                    std::error_code ec;
                    for (auto& Entry : std::filesystem::directory_iterator(CurrentPath.c_str(), ec))
                    {
                        if (ec || !Entry.is_directory())
                        {
                            continue;
                        }

                        FString Path = Entry.path().generic_string().c_str();
                        FString DisplayName = Entry.path().filename().string().c_str();
                        auto* ChildItem = ParentItem->AddChild<FContentBrowserListViewItem>(ParentItem, Path, DisplayName);

                        if (Entry.path() == SelectedPath.c_str())
                        {
                            OutlinerListView.SetSelection(ChildItem, OutlinerContext);
                        }

                        AddChildrenRecursive(ChildItem, Path);
                    }
                };

                AddChildrenRecursive(RootItem, PhysicalRootStr);
            }
        };



        OutlinerContext.ItemSelectedFunction = [this] (FTreeListViewItem* Item)
        {
            if (Item == nullptr)
            {
                return;
            }
            
            FContentBrowserListViewItem* ContentItem = static_cast<FContentBrowserListViewItem*>(Item);
            
            SelectedPath = ContentItem->GetPath();

            RefreshContentBrowser();
        };

        OutlinerListView.MarkTreeDirty();
        ContentBrowserTileView.MarkTreeDirty();
    }

    void FContentBrowserEditorTool::Update(const FUpdateContext& UpdateContext)
    {
        
    }

    void FContentBrowserEditorTool::EndFrame()
    {
        bool bWroteSomething = false;
        
        for (FPendingDestroy& Destroy : PendingDestroy)
        {
            if (CPackage::DestroyPackage(Destroy.PendingDestroy))
            {
                FString PackagePathWithExt = Destroy.PendingDestroy + ".lasset";
                std::filesystem::remove(PackagePathWithExt.c_str());
                bWroteSomething = true;
            }
        }
        
        for (FPendingRename& Rename : PendingRenames)
        {
            if (HandleRenameEvent(Rename.OldName, Rename.NewName))
            {
                bWroteSomething = true;
                ImGuiX::Notifications::NotifySuccess("Renamed to \"%s\"", Rename.NewName.c_str());
            }
            else
            {
                ImGuiX::Notifications::NotifyError("Failed to rename asset");
            }
        }

        if (bWroteSomething)
        {
            RefreshContentBrowser();
        }

        PendingDestroy.clear();
        PendingRenames.clear();
    }

    void FContentBrowserEditorTool::InitializeDockingLayout(ImGuiID InDockspaceID, const ImVec2& InDockspaceSize) const
    {
        ImGuiID topDockID = 0, bottomLeftDockID = 0, bottomCenterDockID = 0, bottomRightDockID = 0;
        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Down, 0.5f, &bottomCenterDockID, &topDockID);
        ImGui::DockBuilderSplitNode(bottomCenterDockID, ImGuiDir_Right, 0.66f, &bottomCenterDockID, &bottomLeftDockID);
        ImGui::DockBuilderSplitNode(bottomCenterDockID, ImGuiDir_Right, 0.5f, &bottomRightDockID, &bottomCenterDockID);

        ImGui::DockBuilderDockWindow(GetToolWindowName("Content").c_str(), bottomCenterDockID);
    }

    void FContentBrowserEditorTool::DrawToolMenu(const FUpdateContext& UpdateContext)
    {
        if (ImGui::BeginMenu(LE_ICON_FILTER " Filter"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));

            for (auto& [Name, State] : FilterState)
            {
                if (ImGui::Checkbox(Name.c_str(), &State))
                {
                    RefreshContentBrowser();
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::EndMenu();
        }
    }

    void FContentBrowserEditorTool::HandleContentBrowserDragDrop(FContentBrowserTileViewItem* Drop, FContentBrowserTileViewItem* Payload)
    {
        /** If the payload is a folder, and is empty, we don't need to do much besides moving the folder */
        if (Payload->IsDirectory() && Paths::Exists(Payload->GetPath()))
        {
            if (std::filesystem::is_empty(Payload->GetPath().c_str()))
            {
                std::filesystem::path SourcePath = Payload->GetPath().c_str();
                FString SourceFileName = SourcePath.filename().generic_string().c_str();
            
                FString DestPath = Drop->GetPath() + "/" + SourceFileName;
                
                try
                {
                    std::filesystem::rename(SourcePath, DestPath.c_str());
                    LOG_INFO("[ContentBrowser] Moved folder {0} -> {1}", SourcePath.string(), DestPath);
                }
                catch (const std::filesystem::filesystem_error& e)
                {
                    LOG_ERROR("[ContentBrowser] Failed to move folder: {0}", e.what());
                }
            }
            else
            {
                //... Moving all assets in a folder :(
            }
        }


        if (!Payload->IsDirectory())
        {
            FString OldPath = Payload->GetPath();
            FString NewPath = Drop->GetPath() + "/" + Payload->GetAssetData().AssetName.ToString();
            PendingRenames.emplace_back(FPendingRename{OldPath, NewPath});
        }
    }

    void FContentBrowserEditorTool::TryImport(const FString& Path)
    {
        TVector<CAssetDefinition*> Definitions;
        CAssetDefinitionRegistry::Get()->GetAssetDefinitions(Definitions);
        for (CAssetDefinition* Definition : Definitions)
        {
            if (!Definition->CanImport())
            {
                continue;
            }
        
            FString Ext = Paths::GetExtension(Path);
            if (!Definition->IsExtensionSupported(Ext))
            {
                continue;
            }
        
            CFactory* Factory = Definition->GetFactory();
        
            FString NoExtFileName = Paths::FileName(Path, true);
            FString PathString = Paths::Combine(SelectedPath.c_str(), NoExtFileName.c_str());
        
            Paths::AddPackageExtension(PathString);
            PathString = CPackage::MakeUniquePackagePath(PathString);
            PathString = Paths::RemoveExtension(PathString);
        
            if (Factory->HasImportDialogue())
            {
                ToolContext->PushModal("Import", {700, 800}, [this, Factory, Path, PathString](const FUpdateContext&)
                {
                    bool bShouldClose = CFactory::ShowImportDialogue(Factory, Path, PathString);
                    if (bShouldClose)
                    {
                        ImGuiX::Notifications::NotifySuccess("Successfully Imported: \"%s\"", PathString.c_str());
                    }
            
                    return bShouldClose;
                });
            }
            else
            {
                Task::AsyncTask(1, 1, [this, Factory, Path, PathString] (uint32, uint32, uint32)
                {
                    Factory->TryImport(Path, PathString);
                    ImGuiX::Notifications::NotifySuccess("Successfully Imported: \"%s\"", PathString.c_str());
                });
            }
        }
    }

    void FContentBrowserEditorTool::OnWindowDropped(FWindow*, int NumPaths, const char** Paths)
    {
        ImVec2 DropCursor = ImVec2(Input::GetMousePosition().x, Input::GetMousePosition().y);
        
        for (int i = 0; i < NumPaths; ++i)
        {
            PendingDrops.emplace_back(FPendingOSDrop{ FString(Paths[i]), DropCursor });
        }
    }

    bool FContentBrowserEditorTool::HandleRenameEvent(const FString& OldPath, FString NewPath)
    {
        try
        {
            bool bDirectory = std::filesystem::is_directory(OldPath.c_str());
            
            if (!bDirectory)
            {
                NewPath += ".lasset";
            }
        
            if (std::filesystem::exists(NewPath.c_str()))
            {
                LOG_ERROR("Destination path already exists: {0}", NewPath);
                return false;
            }
            
            if (!bDirectory)
            {
                if (CPackage* OldPackage = CPackage::LoadPackage(OldPath.c_str()))
                {
                    // Make sure nothing is currently loading before executing this rename.
                    GEngine->GetEngineSubsystem<FAssetManager>()->FlushAsyncLoading();
                    
                    /** We need all objects to be loaded to rename a package */
                    OldPackage->LoadObjects();

                    bool bCreateRedirectors = true;
                    
                    FAssetRegistry* AssetRegistry = GEngine->GetEngineSubsystem<FAssetRegistry>();
                    const THashSet<FName>& References = AssetRegistry->GetReferences(OldPackage->GetName());

                    //@TODO bCreateRedirectors = References.empty();
                    
                    FString OldAssetName = Paths::FileName(OldPath, true);
                    FString NewAssetName = Paths::FileName(NewPath, true);

                    CPackage* NewPackage = CPackage::CreatePackage(NewPath);
                    
                    TVector<CObject*> Objects;
                    GetObjectsWithPackage(OldPackage, Objects);

                    for (CObject* Object : Objects)
                    {
                        if (Object->IsAsset())
                        {
                            Object->Rename(NewAssetName, NewPackage, bCreateRedirectors);
                            AssetRegistry->AssetRenamed(Object, OldPath);
                        }
                        else
                        {
                            Object->Rename(Object->GetName(), NewPackage);
                        }
                    }

                    CPackage::SavePackage(OldPackage, OldPath);
                    CPackage::SavePackage(NewPackage, NewPath);
                }
            }
            else
            {
                std::filesystem::rename(OldPath.c_str(), NewPath.c_str());
            }
            
            return true;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            LOG_ERROR("Failed to rename file: {0}", e.what());
            return false;
        }
    }
    
    void FContentBrowserEditorTool::DrawDirectoryBrowser(const FUpdateContext& Contxt, bool bIsFocused, ImVec2 Size)
    {
        ImGui::BeginChild("Directories", Size);

        OutlinerListView.Draw(OutlinerContext);
        
        ImGui::EndChild();
    }

    void FContentBrowserEditorTool::DrawContentBrowser(const FUpdateContext& Context, bool bIsFocused, ImVec2 Size)
    {
        FString Path = Paths::GetEngineResourceDirectory();
        constexpr float Padding = 10.0f;

        ImVec2 AdjustedSize = ImVec2(Size.x - 2 * Padding, Size.y - 2 * Padding);

        ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(Padding, Padding));

        ImGui::BeginChild("Content", AdjustedSize, true, ImGuiWindowFlags_None);
        
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("ContentContextMenu");
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 100.0f), ImVec2(0.0f, 0.0f));
        
        if (ImGui::BeginPopup("ContentContextMenu"))
        {
            {
                const char* FolderIcon = LE_ICON_FOLDER;
                FString MenuItemName = FString(FolderIcon) + " " + "New Folder";
                if (ImGui::MenuItem(MenuItemName.c_str()))
                {
                    FString PathString = FString(SelectedPath + "/NewFolder");
                    PathString = Paths::MakeUniquePath(PathString);
                    std::filesystem::create_directory(PathString.c_str());
                    RefreshContentBrowser();
                }
            }
            
            {
                const char* ImportIcon = LE_ICON_FILE_IMPORT;
                FString MenuItemName = FString(ImportIcon) + " " + "Import Asset";
                if (ImGui::MenuItem(MenuItemName.c_str()))
                {
                    FString SelectedFile;
                    const char* Filter = "Supported Assets (*.png;*.jpg;*.fbx;*.gltf;*.glb)\0*.png;*.jpg;*.fbx;*.gltf;*.glb\0All Files (*.*)\0*.*\0";
                    if (Platform::OpenFileDialogue(SelectedFile, "Import Asset", Filter))
                    {
                        TVector<CAssetDefinition*> Definitions;
                        CAssetDefinitionRegistry::Get()->GetAssetDefinitions(Definitions);
                        for (CAssetDefinition* Definition : Definitions)
                        {
                            if (!Definition->CanImport())
                            {
                                continue;
                            }

                            FString Ext = Paths::GetExtension(SelectedFile);
                            if (!Definition->IsExtensionSupported(Ext))
                            {
                                continue;
                            }
                        
                            CFactory* Factory = Definition->GetFactory();
                        
                            FString NoExtFileName = Paths::FileName(SelectedFile, true);
                            FString PathString = Paths::Combine(SelectedPath.c_str(), NoExtFileName.c_str());
                        
                            Paths::AddPackageExtension(PathString);
                            PathString = CPackage::MakeUniquePackagePath(PathString);
                            PathString = Paths::RemoveExtension(PathString);
                        
                            if (Factory->HasImportDialogue())
                            {
                                ToolContext->PushModal("Import", {700, 800}, [this, Factory, SelectedFile, PathString](const FUpdateContext& DrawContext)
                                {
                                    bool bShouldClose = CFactory::ShowImportDialogue(Factory, SelectedFile, PathString);
                                    if (bShouldClose)
                                    {
                                        ImGuiX::Notifications::NotifySuccess("Successfully Imported: \"%s\"", PathString.c_str());
                                    }
                            
                                    return bShouldClose;
                                });
                            }
                            else
                            {
                                Task::AsyncTask(1, 1, [this, Factory, SelectedFile, PathString] (uint32 Start, uint32 End, uint32 ThreadNum_)
                                {
                                    Factory->TryImport(SelectedFile, PathString);
                                    ImGuiX::Notifications::NotifySuccess("Successfully Imported: \"%s\"", PathString.c_str());
                                });
                            }
                        }
                    }
                }
            }
            
            
            const char* FileIcon = LE_ICON_PLUS;
            const char* File = "New Asset";

            ImGui::Separator();
            
            FString MenuItemName = FString(FileIcon) + " " + File;

            if (ImGui::BeginMenu(MenuItemName.c_str()))
            {
                TVector<CAssetDefinition*> Definitions;
                CAssetDefinitionRegistry::Get()->GetAssetDefinitions(Definitions);
                for (CAssetDefinition* Definition : Definitions)
                {
                    if (Definition->CanImport())
                    {
                        continue;
                    }
                    
                    FString DisplayName = Definition->GetAssetDisplayName();
                    if (ImGui::MenuItem(DisplayName.c_str()))
                    {
                        CFactory* Factory = Definition->GetFactory();
                        FString PathString = Paths::Combine(SelectedPath.c_str(), Factory->GetDefaultAssetCreationName(PathString).c_str());
                        Paths::AddPackageExtension(PathString);
                        PathString = CPackage::MakeUniquePackagePath(PathString);
                        PathString = Paths::RemoveExtension(PathString);

                        if (Factory->HasCreationDialogue())
                        {
                            ToolContext->PushModal("Create New", {500, 500}, [this, Factory, PathString](const FUpdateContext& DrawContext)
                            {
                                bool bShouldClose = CFactory::ShowCreationDialogue(Factory, PathString);
                                if (bShouldClose)
                                {
                                    ImGuiX::Notifications::NotifySuccess("Successfully Created: \"%s\"", PathString.c_str());
                                }
                        
                                return bShouldClose;
                            });
                        }
                        else
                        {
                            CObject* Object = Factory->TryCreateNew(PathString);
                            if (Object)
                            {
                                ImGuiX::Notifications::NotifySuccess("Successfully Created: \"%s\"", PathString.c_str());
                            }
                            else
                            {
                                ImGuiX::Notifications::NotifyError("Failed to create new: \"%s\"", PathString.c_str());

                            }
                        }
                    }
                }
                
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
        

        ImGui::BeginHorizontal("Breadcrumbs");

        std::filesystem::path SelectedFSPath = SelectedPath.c_str();
        std::filesystem::path current;
        bool bStarted = false;

        for (auto it = SelectedFSPath.begin(); it != SelectedFSPath.end(); ++it)
        {
            current /= *it;

            if (!bStarted)
            {
                if (it->string() == "Content")
                {
                    bStarted = true;
                }
                else
                {
                    continue;
                }
            }

            ImGui::PushID((int)std::distance(SelectedFSPath.begin(), it));
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
                if (ImGui::Button(it->string().c_str()))
                {
                    SelectedPath = current.generic_string().c_str();
                    ContentBrowserTileView.MarkTreeDirty();
                }
                ImGui::PopStyleVar(2);
            }
            ImGui::PopID();

            auto next = it;
            ++next;
            
            if (next != SelectedFSPath.end())
            {
                ImGui::TextUnformatted(LE_ICON_ARROW_RIGHT);
            }
        }

        ImGui::EndHorizontal();

        ImGuiTextFilter SearchFilter;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
        SearchFilter.Draw("##Search", 450.0f);
        ImGui::PopStyleVar();

        ImGui::Separator();
        
        ContentBrowserTileView.Draw(ContentBrowserTileViewContext);

        ImVec2 ChildMin = ImGui::GetWindowPos();
        ImVec2 ChildMax = ImVec2(ChildMin.x + ImGui::GetWindowWidth(), ChildMin.y + ImGui::GetWindowHeight());
        
        ImRect Rect(ChildMin, ChildMax);

        for (FPendingOSDrop& Drop : PendingDrops)
        {
            if (Rect.Contains(Drop.MousePos))
            {
                TryImport(Drop.Path);
            }
        }

        PendingDrops.clear();
        
        ImGui::EndChild();
    
    }
}
