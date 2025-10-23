#pragma once

#include "EdGraphNode.h"
#include "Containers/Array.h"
#include "Containers/Function.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "EdNodeGraph.generated.h"
#include "imgui-node-editor/imgui_node_editor.h"

namespace Lumina
{
    class CEdGraphNode;
}

namespace Lumina
{
    LUM_CLASS()
    class CEdNodeGraph : public CObject
    {
        GENERATED_BODY()
        
    public:
        
        struct FAction
        {
            FString             ActionName;
            TFunction<void()>   ActionCallback;
        };

        struct FNodeFactory
        {
            FString Name;
            FString Tooltip;
            TFunction<CEdGraphNode*()> CreationCallback;
        };

        
        CEdNodeGraph();
        ~CEdNodeGraph() override;

        virtual void Initialize();
        virtual void Shutdown();
        void Serialize(FArchive& Ar) override;
        
        void DrawGraph();
        virtual void DrawGraphContextMenu();
        virtual void OnDrawGraph();
        
        void RegisterGraphAction(const FString& ActionName, const TFunction<void()>& ActionCallback);

        virtual void ValidateGraph()  { }
        
        virtual CEdGraphNode* CreateNode(CClass* NodeClass);

        virtual CEdGraphNode* OnNodeRemoved(CEdGraphNode* Node) { return nullptr; }

        void SetNodeSelectedCallback(const TFunction<void(CEdGraphNode*)>& Callback) { NodeSelectedCallback = Callback; }


    private:

        static bool GraphSaveSettings(const char* data, size_t size, ax::NodeEditor::SaveReasonFlags reason, void* userPointer);
        static size_t GraphLoadSettings(char* data, void* userPointer);
        
    public:

        void RegisterGraphNode(CClass* InClass);
        
        uint64 AddNode(CEdGraphNode* InNode);

        LUM_PROPERTY()
        TVector<TObjectHandle<CEdGraphNode>>            Nodes;
        
        LUM_PROPERTY()
        TVector<uint16>                                 Connections;

        LUM_PROPERTY()
        FString GraphSaveData;
        
        THashSet<CClass*>                               SupportedNodes;
        TVector<FAction>                                Actions;
        TQueue<CEdGraphNode*>                           NodesToDestroy;
        
        TFunction<void(CEdGraphNode*)>                  NodeSelectedCallback;
    
    private:

        ax::NodeEditor::EditorContext* Context = nullptr;
    };
    
    
}
