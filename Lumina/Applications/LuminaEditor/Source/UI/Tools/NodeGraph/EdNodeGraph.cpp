#include "EdNodeGraph.h"

#include "EdGraphNode.h"
#include "Core/Object/Class.h"
#include <Core/Reflection/Type/LuminaTypes.h>
#include "Drawing.h"
#include "imgui_internal.h"
#include "Core/Object/Cast.h"
#include "Core/Object/Package/Package.h"
#include "Core/Profiler/Profile.h"
#include "EASTL/sort.h"
#include "imgui-node-editor/imgui_node_editor_internal.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"
#include "Tools/UI/ImGui/ImGuiX.h"

namespace Lumina
{
    static void DrawPinIcon(bool bConnected, int Alpha, ImVec4 Color)
    {
        EIconType iconType = EIconType::Circle;
        Color.w = Alpha / 255.0f;
        
        Icon(ImVec2(24.f, 24.0f), iconType, bConnected, Color, ImColor(32, 32, 32, Alpha));
    }
    
    CEdNodeGraph::CEdNodeGraph()
        : NodeSelectedCallback()
    {
    }

    CEdNodeGraph::~CEdNodeGraph()
    {
    }

    bool CEdNodeGraph::GraphSaveSettings(const char* data, size_t size, ax::NodeEditor::SaveReasonFlags reason, void* userPointer)
    {
        CEdNodeGraph* ThisGraph = (CEdNodeGraph*)userPointer;
        
        if (reason != ax::NodeEditor::SaveReasonFlags::None && !ThisGraph->bFirstDraw)
        {
            ThisGraph->GetPackage()->MarkDirty();
            ThisGraph->GraphSaveData.assign(data, size);
        }
        
        return true;
    }
    
    size_t CEdNodeGraph::GraphLoadSettings(char* data, void* userPointer)
    {
        CEdNodeGraph* ThisGraph = (CEdNodeGraph*)userPointer;
        if (data)
        {
            memcpy(data, ThisGraph->GraphSaveData.data(), ThisGraph->GraphSaveData.size());
        }
        return ThisGraph->GraphSaveData.size();
    }
    

    void CEdNodeGraph::Initialize()
    {
        ax::NodeEditor::Config config;
        config.EnableSmoothZoom = true;
        config.UserPointer = this;
        config.SaveSettings = GraphSaveSettings;
        config.LoadSettings = GraphLoadSettings;
        config.SettingsFile = nullptr;
        Context = ax::NodeEditor::CreateEditor(&config);
    }

    void CEdNodeGraph::Shutdown()
    {
        ax::NodeEditor::DestroyEditor(Context);
        Context = nullptr;
    }

    void CEdNodeGraph::Serialize(FArchive& Ar)
    {
        Super::Serialize(Ar);
    }

    void CEdNodeGraph::DrawGraph()
    {
        LUMINA_PROFILE_SCOPE();
        using namespace ax;
        
        NodeEditor::SetCurrentEditor(Context);
        NodeEditor::Begin(GetQualifiedName().c_str());
    
        Graph::GraphNodeBuilder NodeBuilder;
        
        THashMap<uint64, uint64> NodeIDToIndex;
        TVector<TPair<CEdNodeGraphPin*, CEdNodeGraphPin*>> Links;
        Links.reserve(40);
    
        uint32 Index = 0;
        for (CEdGraphNode* Node : Nodes)
        {
            NodeIDToIndex[Node->GetNodeID()] = Index;
            
            NodeBuilder.Begin(Node->GetNodeID());
            
            if (!Node->WantsTitlebar())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            }
            
            NodeBuilder.Header(ImGui::ColorConvertU32ToFloat4(Node->GetNodeTitleColor()));
            
            if (!Node->WantsTitlebar())
            {
                ImGui::PopStyleVar();
            }
            
            ImGui::Spring(0);
            Node->DrawNodeTitleBar();
            ImGui::Spring(1);
            ImGui::Dummy(ImVec2(Node->GetMinNodeTitleBarSize()));
            ImGui::Spring(0);
            NodeBuilder.EndHeader();
    
            if (Node->GetInputPins().empty())
            {
                ImGui::BeginVertical("inputs", ImVec2(0,0), 0.0f);
                ImGui::Dummy(ImVec2(0,0));
                ImGui::EndVertical();
            }
            
            for (CEdNodeGraphPin* InputPin : Node->GetInputPins())
            {
                for (CEdNodeGraphPin* Connection : InputPin->GetConnections())
                {
                    Links.emplace_back(TPair(InputPin, Connection));
                }
                
                NodeBuilder.Input(InputPin->GetGUID());
    
                ImGui::PushID(InputPin);
                {
                    ImVec4 PinColor = ImGui::ColorConvertU32ToFloat4(InputPin->GetPinColor());
                    if (Node->HasError())
                    {
                        PinColor = ImVec4(255.0f, 0.0f, 0.0f, 255.0f);
                    }
                    DrawPinIcon(InputPin->HasConnection(), 255.0f, PinColor);
                    ImGui::Spring(0);
    
                    ImGui::TextUnformatted(InputPin->GetPinName().c_str());
                    
                    ImGui::Spring(0);
                }
                ImGui::PopID();
                
                NodeBuilder.EndInput();
            }
    
            NodeBuilder.Middle();
            Node->DrawNodeBody();
            
            for (CEdNodeGraphPin* OutputPin : Node->GetOutputPins())
            {
                NodeBuilder.Output(OutputPin->GetGUID());
                
                ImGui::PushID(OutputPin);
                {
                    ImGui::Spring(0);
    
                    ImGui::Spring(1, 1);
                    ImGui::TextUnformatted(OutputPin->GetPinName().c_str());
                    ImGui::Spring(0);
                    
                    ImVec4 PinColor = ImGui::ColorConvertU32ToFloat4(OutputPin->GetPinColor());
                    if (Node->HasError())
                    {
                        PinColor = ImVec4(255.0f, 0.0f, 0.0f, 255.0f);
                    }
                    DrawPinIcon(OutputPin->HasConnection(), 255.0f, PinColor);
                }
                ImGui::PopID();
    
                NodeBuilder.EndOutput();
            }
            
            NodeBuilder.End(Node->WantsTitlebar());
            Index++;
        }
    
        // Background context menu
        NodeEditor::Suspend();
        {
            if (NodeEditor::ShowBackgroundContextMenu())
            {
                ImGui::OpenPopup("Create New Node");
            }
            
            if (ImGui::BeginPopup("Create New Node"))
            {
                DrawGraphContextMenu();
                ImGui::EndPopup();
            }
        }
        NodeEditor::Resume();
    
        // Handle node selection
        bool bAnyNodeSelected = false;
        for (CEdGraphNode* Node : Nodes)
        {
            if (NodeEditor::IsNodeSelected(Node->GetNodeID()))
            {
                bAnyNodeSelected = true;
                if (NodeSelectedCallback)
                {
                    NodeSelectedCallback(Node);
                }
            }
        }
    
        if (!bAnyNodeSelected && NodeSelectedCallback)
        {
            NodeSelectedCallback(nullptr);
        }
        
        // Draw all links
        uint32 LinkID = 1;
        for (auto& [Start, End] : Links)
        {
            NodeEditor::Link(LinkID++, Start->GetGUID(), End->GetGUID());
        }
        
        // Handle link creation
        if (NodeEditor::BeginCreate())
        {
            NodeEditor::PinId StartPinID, EndPinID;
            if (NodeEditor::QueryNewLink(&StartPinID, &EndPinID))
            {
                if (StartPinID && EndPinID && StartPinID != EndPinID)
                {
                    if (NodeEditor::AcceptNewItem())
                    {
                        CEdNodeGraphPin* StartPin = nullptr;
                        CEdNodeGraphPin* EndPin = nullptr;
    
                        for (CEdGraphNode* Node : Nodes)
                        {
                            StartPin = Node->GetPin(StartPinID.Get(), ENodePinDirection::Output);
                            if (StartPin)
                            {
                                break;
                            }
                        }
    
                        for (CEdGraphNode* Node : Nodes)
                        {
                            EndPin = Node->GetPin(EndPinID.Get(), ENodePinDirection::Input);
                            if (EndPin)
                            {
                                break;
                            }
                        }
    
                        bool bValid = StartPin && EndPin && 
                                      StartPin != EndPin && 
                                      StartPin->OwningNode != EndPin->OwningNode &&
                                      !EndPin->HasConnection();
    
                        if (bValid)
                        {
                            StartPin->AddConnection(EndPin);
                            EndPin->AddConnection(StartPin);
                            ValidateGraph();
                        }
                    }
                }
            }
        }

        NodeEditor::EndCreate();

        
        // Handle deletion
        if (NodeEditor::BeginDelete())
        {
            // Handle node deletion
            NodeEditor::NodeId NodeId = 0;
            while (NodeEditor::QueryDeletedNode(&NodeId))
            {
                // Unfortunately the way we do this now is a bit gross, it's too much of a pain to keep these nodes and the internal NodeEditor nodes in sync.
                // This is the way it's done in the examples, and even though it's essentially O(n^2), it seems to be working correctly.
                // Realistically it shouldn't matter too much, and should be fine.
                auto NodeItr = eastl::find_if(Nodes.begin(), Nodes.end(), [NodeId] (const TObjectHandle<CEdGraphNode>& A)
                {
                    return A->GetNodeID() == NodeId.Get();
                });

                if (NodeItr != Nodes.end())
                {
                    CEdGraphNode* Node = *NodeItr;
                    if (!Node->IsDeletable() || !NodeEditor::AcceptDeletedItem())
                    {
                        continue;
                    }
                    
                    for (CEdNodeGraphPin* Pin : Node->GetInputPins())
                    {
                        if (Pin->HasConnection())
                        {
                            TVector<CEdNodeGraphPin*> PinConnections = Pin->GetConnections();
                            for (CEdNodeGraphPin* ConnectedPin : PinConnections)
                            {
                                ConnectedPin->DisconnectFrom(Pin);
                            }
                            Pin->ClearConnections();
                        }
                    }
        
                    for (CEdNodeGraphPin* Pin : Node->GetOutputPins())
                    {
                        if (Pin->HasConnection())
                        {
                            TVector<CEdNodeGraphPin*> PinConnections = Pin->GetConnections();
                            for (CEdNodeGraphPin* ConnectedPin : PinConnections)
                            {
                                ConnectedPin->DisconnectFrom(Pin);
                            }
                            Pin->ClearConnections();
                        }
                    }
                    
                    Nodes.erase(NodeItr);

                    DestroyCObject(Node);
                    Node = nullptr;
                    
                    ValidateGraph();
                }
            }
            
            NodeEditor::LinkId DeletedLinkId;
            while (NodeEditor::QueryDeletedLink(&DeletedLinkId))
            {
                if (NodeEditor::AcceptDeletedItem())
                {
                    uint32 LinkIndex = DeletedLinkId.Get() - 1;
                    if (LinkIndex < Links.size())
                    {
                        const TPair<CEdNodeGraphPin*, CEdNodeGraphPin*>& Pair = Links[LinkIndex];
                        Pair.first->RemoveConnection(Pair.second);
                        Pair.second->RemoveConnection(Pair.first);
                        ValidateGraph();
                    }
                }
            }
        }
        NodeEditor::EndDelete();
        
        NodeEditor::End();
        NodeEditor::SetCurrentEditor(nullptr);

        bFirstDraw = false;
    }


    void CEdNodeGraph::DrawGraphContextMenu()
    {
        const ImVec2 PopupSize(320, 450);
        const ImVec2 SearchBarPadding(12, 10);
        const ImVec2 CategoryPadding(8, 6);
        const float ItemHeight = 28.0f;
        const float CategorySpacing = 4.0f;
        static char SearchBuffer[256] = "";

        ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Always);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SearchBarPadding);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.16f, 0.18f, 1.0f));
            
            ImGui::SetCursorPos(ImVec2(12, 12));
            ImGui::PushItemWidth(PopupSize.x - 24);

            
            bool SearchChanged = ImGui::InputTextWithHint("##NodeSearch", LE_ICON_BOOK_SEARCH " Search nodes...", 
                SearchBuffer, 
                IM_ARRAYSIZE(SearchBuffer),
                ImGuiInputTextFlags_AutoSelectAll);
            
            ImGui::PopItemWidth();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }
        
        // Separator line
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        
        
        struct FNodeInfo
        {
            CClass* NodeClass;
            CEdGraphNode* CDO;
            int MatchScore;
        };
        
        THashMap<FName, TVector<FNodeInfo>> CategoryMap;
        TVector<FName> CategoryOrder;
        int TotalMatches = 0;
        
        for (CClass* NodeClass : SupportedNodes)
        {
            CEdGraphNode* CDO = Cast<CEdGraphNode>(NodeClass->GetDefaultObject());
            FName Category = CDO->GetNodeCategory().c_str();
            

            if (CategoryMap.find(Category) == CategoryMap.end())
            {
                CategoryOrder.push_back(Category);
            }
            
            int Score = 100;
            if (SearchBuffer[0] != '\0' && strncmp(CDO->GetNodeDisplayName().c_str(), SearchBuffer, strlen(SearchBuffer)) == 0)
            {
                Score += 50;
            }
            
            CategoryMap[Category].push_back({NodeClass, CDO, Score});
            TotalMatches++;
        }
        
        // Sort nodes within each category by relevance
        for (auto& [Category, NodesInMap] : CategoryMap)
        {
            eastl::sort(NodesInMap.begin(), NodesInMap.end(), [](const FNodeInfo& A, const FNodeInfo& B)
            {
                if (A.MatchScore != B.MatchScore)
                {
                    return A.MatchScore > B.MatchScore;
                }
                return strcmp(A.CDO->GetNodeDisplayName().c_str(), B.CDO->GetNodeDisplayName().c_str()) < 0;
            });
        }

        constexpr float HeaderHeight = 54; // Search bar + separator
        constexpr float FooterHeight = 32; // Status bar
        ImVec2 ChildSize = ImVec2(PopupSize.x, PopupSize.y - HeaderHeight - FooterHeight);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, CategorySpacing));
        
        if (ImGui::BeginChild("##NodeList", ChildSize, false, 
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.08f, 0.08f, 0.10f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.35f, 0.35f, 0.37f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.45f, 0.45f, 0.47f, 1.0f));
            
            if (ImGui::BeginChild("##ScrollRegion", ImVec2(0, 0), false))
            {
                bool IsFirstCategory = true;
                
                for (const FName& Category : CategoryOrder)
                {
                    const auto& NodeInfos = CategoryMap[Category];
                    
                    if (!IsFirstCategory)
                    {
                        ImGui::Dummy(ImVec2(0, CategorySpacing));
                    }
                    IsFirstCategory = false;
                    
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, CategoryPadding);
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
                    
                    ImGui::SetCursorPosX(4);
                    bool IsOpen = ImGui::CollapsingHeader(Category.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
                    
                    ImGui::PopStyleColor(3);
                    ImGui::PopStyleVar();
                    
                    if (IsOpen)
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                        
                        for (const FNodeInfo& Info : NodeInfos)
                        {
                            ImGui::PushID(Info.CDO);
                            
                            ImVec4 AccentColor = ImVec4(0.8f, 0.4f, 0.2f, 1.0f);
                            
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.16f, 0.16f, 0.18f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, AccentColor * ImVec4(1, 1, 1, 0.3f));
                            
                            ImGui::SetCursorPosX(16);
                            
                            ImDrawList* DrawList = ImGui::GetWindowDrawList();
                            ImVec2 CursorPos = ImGui::GetCursorScreenPos();
                            DrawList->AddRectFilled(ImVec2(CursorPos.x - 8, CursorPos.y + ItemHeight * 0.3f), ImVec2(CursorPos.x - 5, CursorPos.y + ItemHeight * 0.7f),
                                ImGui::GetColorU32(AccentColor),
                                2.0f
                            );
                            
                            if (ImGui::Selectable(Info.CDO->GetNodeDisplayName().c_str(), false, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0, ItemHeight)))
                            {
                                CEdGraphNode* NewNode = CreateNode(Info.NodeClass);
                                ax::NodeEditor::SetNodePosition(NewNode->GetNodeID(), ax::NodeEditor::ScreenToCanvas(ImGui::GetMousePosOnOpeningCurrentPopup()));
                                ImGui::CloseCurrentPopup();
                            }
                            
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            {
                                ImGui::BeginTooltip();
                                ImGui::PushStyleColor(ImGuiCol_Text, AccentColor);
                                ImGui::Text("%s", Info.CDO->GetNodeDisplayName().c_str());
                                ImGui::PopStyleColor();
                                
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                                ImGui::TextWrapped("%s", Info.CDO->GetNodeTooltip().c_str());
                                ImGui::PopStyleColor();
                                ImGui::EndTooltip();
                            }
                            
                            ImGui::PopStyleColor(3);
                            ImGui::PopID();
                        }
                        
                        ImGui::PopStyleVar(2);
                    }
                }
                
                if (TotalMatches == 0 && SearchBuffer[0] != '\0')
                {
                    ImGui::SetCursorPosY(ChildSize.y * 0.4f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    
                    const char* NoResultsText = "No nodes found";
                    float TextWidth = ImGui::CalcTextSize(NoResultsText).x;
                    ImGui::SetCursorPosX((PopupSize.x - TextWidth) * 0.5f);
                    ImGui::Text("%s", NoResultsText);
                    
                    ImGui::PopStyleColor();
                }
                
                ImGui::EndChild();
            }
            
            ImGui::PopStyleColor(4);
            ImGui::EndChild();
        }
        
        ImGui::PopStyleVar(2);
        
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::SetCursorPos(ImVec2(12, PopupSize.y - 24));
        
        if (TotalMatches > 0)
        {
            ImGui::Text("%d node%s found", TotalMatches, TotalMatches == 1 ? "" : "s");
        }
        else if (SearchBuffer[0] == '\0')
        {
            ImGui::Text("%d node%s available", (int)SupportedNodes.size(), SupportedNodes.size() == 1 ? "" : "s");
        }
        
        ImGui::PopStyleColor();
        
        ImGui::PopStyleVar(2);
    }


    void CEdNodeGraph::OnDrawGraph()
    {
        
    }

    void CEdNodeGraph::RegisterGraphAction(const FString& ActionName, const TFunction<void()>& ActionCallback)
    {
        FAction NewAction;
        NewAction.ActionName = ActionName;
        NewAction.ActionCallback = ActionCallback;

        Actions.push_back(NewAction);
    }

    CEdGraphNode* CEdNodeGraph::CreateNode(CClass* NodeClass)
    {
        CEdGraphNode* NewNode = NewObject<CEdGraphNode>(NodeClass);
        AddNode(NewNode);
        return NewNode;
    }


    void CEdNodeGraph::RegisterGraphNode(CClass* InClass)
    {
        if (SupportedNodes.find(InClass) == SupportedNodes.end())
        {
            SupportedNodes.emplace(InClass);
        }
    }

    uint64 CEdNodeGraph::AddNode(CEdGraphNode* InNode)
    {
        SIZE_T NewID = NextID++;
        InNode->FullName = InNode->GetNodeDisplayName() + "_" + eastl::to_string(NewID);
        InNode->NodeID = NewID;

        Nodes.push_back(InNode);

        if (!InNode->bWasBuild)
        {
            InNode->BuildNode();
            InNode->bWasBuild = true;
        }
        
        ValidateGraph();

        return NewID;
    }
    
}
