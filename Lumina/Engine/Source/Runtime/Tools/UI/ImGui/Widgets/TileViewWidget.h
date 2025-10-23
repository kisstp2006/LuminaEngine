#pragma once
#include "imgui.h"
#include "Containers/Array.h"
#include "Containers/Function.h"
#include "Containers/Name.h"
#include "Memory/Memory.h"
#include "Memory/Allocators/Allocator.h"

namespace Lumina
{

    class LUMINA_API FTileViewItem
    {

        friend class FTileViewWidget;
    public:
        
        virtual ~FTileViewItem() = default;

        FTileViewItem(FTileViewItem* InParent)
            : bExpanded(false)
            , bVisible(false)
            , bSelected(false)
        {}

        virtual FName GetName() const { return {}; }
        
        virtual void DrawTooltip() const { };

        virtual bool HasContextMenu() { return false; }

        virtual ImVec4 GetDisplayColor() const { return {};  }

        virtual void OnSelectionStateChanged() { }

        virtual void SetDragDropPayloadData() const { }
        
        virtual FInlineString GetDisplayName() const
        {
            return GetName().c_str();
        }

    private:

        // Disable copies/moves
        FTileViewItem& operator=(FTileViewItem const&) = delete;
        FTileViewItem& operator=(FTileViewItem&&) = delete;


    protected:
        
        uint8                       bExpanded:1;
        uint8                       bVisible:1;
        uint8                       bSelected:1;
        
    };

    struct LUMINA_API FTileViewContext
    {
        /** Callback to draw any context menus this item may want */
        TFunction<void(const TVector<FTileViewItem*>&)>         DrawItemContextMenuFunction;

        /** Callback to override item drawing */
        TFunction<bool(FTileViewItem*)>                         DrawItemOverrideFunction;

        /** Called when a rebuild of the widget tree is requested */
        TFunction<void(FTileViewWidget*)>                       RebuildTreeFunction;

        /** Called when an item has been selected in the tree */
        TFunction<void(FTileViewItem*)>                         ItemSelectedFunction;

        /** Called when we have a drag-drop operation on a target */
        TFunction<void(FTileViewItem*)>                         DragDropFunction;
    };

    class LUMINA_API FTileViewWidget
    {
    public:

        void Draw(FTileViewContext Context);

        void ClearTree();

        void MarkTreeDirty() { bDirty = true; }

        FORCEINLINE bool IsCurrentlyDrawing() const { return bCurrentlyDrawing; }
        FORCEINLINE bool IsDirty() const { return bDirty; }
        
        template<typename T, typename... Args>
        requires (std::is_base_of_v<FTileViewItem, T> && std::is_constructible_v<T, Args...>)
        FORCEINLINE void AddItemToTree(Args&&... args)
        {
            T* New = Allocator.TAlloc<T>(std::forward<Args>(args)...);
            ListItems.push_back(New);
        }


    private:

        
        void RebuildTree(const FTileViewContext& Context, bool bKeepSelections = false);
        
        void DrawItem(FTileViewItem* ItemToDraw, const FTileViewContext& Context, ImVec2 DrawSize);

        void SetSelection(FTileViewItem* Item, FTileViewContext Context);
        void ClearSelection();
    

    private:

        FBlockLinearAllocator                   Allocator;

        TVector<FTileViewItem*>                 Selections;

        /** Root nodes */
        TVector<FTileViewItem*>                 ListItems;

        uint8                                   bDirty:1;
        uint8                                   bCurrentlyDrawing:1;
    };
}
