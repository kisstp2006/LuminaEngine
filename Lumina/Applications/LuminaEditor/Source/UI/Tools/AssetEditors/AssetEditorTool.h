#pragma once

#include "Core/Object/Class.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "UI/Properties/PropertyTable.h"
#include "UI/Tools/EditorTool.h"

namespace Lumina
{
    class FAssetEditorTool : public FEditorTool
    {
    public:
        
        FAssetEditorTool(IEditorToolContext* Context, const FString& AssetName, CObject* InAsset)
            : FEditorTool(Context, AssetName, nullptr)
            , PropertyTable(FPropertyTable(InAsset, InAsset->GetClass()))
            , bAssetLoadBroadcasted(false)
        {
            Asset = InAsset;
            PropertyTable.RebuildTree();
        }
        
        
        void OnInitialize() override;
        void Deinitialize(const FUpdateContext& UpdateContext) override;
        void Update(const FUpdateContext& UpdateContext) override;
        FString GetToolName() const override;
        virtual void OnAssetLoadFinished() { }
        void OnSave() override;

        FPropertyTable* GetPropertyTable() { return &PropertyTable; }
        
    protected:

        virtual bool ShouldGenerateThumbnailOnLoad() const { return false; }
        virtual void GenerateThumbnailOnLoad();

    protected:

        TObjectPtr<CObject>      Asset;
        FPropertyTable              PropertyTable;
        uint8                       bAssetLoadBroadcasted:1;
    };
}
