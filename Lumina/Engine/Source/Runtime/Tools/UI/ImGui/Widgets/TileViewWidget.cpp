#include "TileViewWidget.h"
#include "Assets/AssetPath.h"
#include "Tools/UI/ImGui/ImGuiX.h"

namespace Lumina
{
    void FTileViewWidget::Draw(FTileViewContext Context)
    {
        if (bDirty)
        {
            RebuildTree(Context);
            return;
        }
    
        float PaneWidth = ImGui::GetContentRegionAvail().x;
        constexpr float ThumbnailSize = 120.0f;
        constexpr float TileSpacing = 20.0f;
        constexpr float TextHeight = 36.0f;
        float CellSize = ThumbnailSize + TileSpacing;
        int ItemsPerRow = std::max(1, int(PaneWidth / CellSize));
    
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(TileSpacing, TileSpacing));
    
        int ItemIndex = 0;
        for (FTileViewItem* Item : ListItems)
        {
            const char* DisplayName = Item->GetDisplayName().c_str();
    
            if (ItemIndex % ItemsPerRow != 0)
            {
                ImGui::SameLine();
            }
    
            ImGui::PushID(Item);
            ImGui::BeginGroup();
    
            DrawItem(Item, Context, ImVec2(ThumbnailSize, ThumbnailSize));
    
            ImFont* Font = ImGui::GetIO().Fonts->Fonts[3];
            ImGui::PushFont(Font);
    
            float WrapWidth = ThumbnailSize;
            ImVec2 TextSize = ImGui::CalcTextSize(DisplayName, nullptr, false, WrapWidth);
            
            float TextPosX = (ThumbnailSize - std::min(TextSize.x, WrapWidth)) * 0.5f;
            
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + TextPosX);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + WrapWidth);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::TextWrapped("%s", DisplayName);
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
    
            ImGui::PopFont();
    
            ImGui::Dummy(ImVec2(0, std::max(0.0f, TextHeight - TextSize.y)));
    
            ImGui::EndGroup();
            ImGui::PopID();
    
            ++ItemIndex;
        }
    
        ImGui::PopStyleVar();
    }
    
    void FTileViewWidget::ClearTree()
    {
        Allocator.Reset();
        ListItems.clear();
    }

    void FTileViewWidget::RebuildTree(FTileViewContext Context, bool bKeepSelections)
    {
        Assert(bDirty)

        TVector<FTileViewItem*> CachedSelections = Selections;
        
        ClearSelection();
        ClearTree();

        if (bKeepSelections)
        {
            for (FTileViewItem* Select : CachedSelections)
            {
                SetSelection(Select, Context);
            }
        }
        
        Context.RebuildTreeFunction(this);

        bDirty = false;
    }

    void FTileViewWidget::DrawItem(FTileViewItem* ItemToDraw, const FTileViewContext& Context, ImVec2 DrawSize)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        if (ItemToDraw->bSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.5f, 0.8f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.65f, 0.95f, 0.7f));
        }
        
        bool wasClicked = false;
        
        if (Context.DrawItemOverrideFunction)
        {
            if (Context.DrawItemOverrideFunction(ItemToDraw))
            {
                SetSelection(ItemToDraw, Context);
                wasClicked = true;
            }
        }
        else
        {
            if (ImGui::Button("##", DrawSize))
            {
                SetSelection(ItemToDraw, Context);
                wasClicked = true;
            }
        }
        
        if (ItemToDraw->bSelected)
        {
            ImGui::PopStyleColor(3);
        }
    
        ImGui::PopStyleVar(2);
    
        // Tooltip
        if (ImGui::BeginItemTooltip())
        {
            ItemToDraw->DrawTooltip();
            ImGui::EndTooltip();
        }
        
        // Context menu
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ItemToDraw->HasContextMenu())
        {
            ImGui::OpenPopup("ItemContextMenu");
        }
    
        // Drag and drop source
        if (ImGui::BeginDragDropSource())
        {
            ItemToDraw->SetDragDropPayloadData();
            if (Context.DrawItemOverrideFunction)
            {
                Context.DrawItemOverrideFunction(ItemToDraw);
            }
            ImGui::EndDragDropSource();
        }
    
        // Drag and drop target
        if (ImGui::BeginDragDropTarget())
        {
            if (Context.DragDropFunction)
            {
                Context.DragDropFunction(ItemToDraw);
            }
            
            ImGui::EndDragDropTarget();
        }
        
        // Context menu popup
        if (ItemToDraw->HasContextMenu())
        {
            if (ImGui::BeginPopupContextItem("ItemContextMenu"))
            {
                TVector<FTileViewItem*> SelectionsToDraw;
                SelectionsToDraw.push_back(ItemToDraw);
                Context.DrawItemContextMenuFunction(SelectionsToDraw);
                
                ImGui::EndPopup();
            }
        }
    }

    void FTileViewWidget::SetSelection(FTileViewItem* Item, FTileViewContext Context)
    {
        bool bWasSelected = Item->bSelected;
        
        ClearSelection();
        
        if (!bWasSelected)
        {
            Selections.push_back(Item);
            Item->bSelected = true;
        }

        Context.ItemSelectedFunction(Item);
        Item->OnSelectionStateChanged();
    }

    void FTileViewWidget::ClearSelection()
    {
        for (FTileViewItem* Item : Selections)
        {
            Assert(Item->bSelected)
            
            Item->bSelected = false;
            Item->OnSelectionStateChanged();    
        }

        Selections.clear();
    }
}
