#include "ConsoleLogEditorTool.h"

#include <algorithm>

#include "Core/Console/ConsoleVariable.h"
#include "EASTL/sort.h"

namespace Lumina
{
    void FConsoleLogEditorTool::OnInitialize()
    {
        CreateToolWindow("Console", [this] (const FUpdateContext& UpdateContext, bool bIsFocused)
        {
           DrawLogWindow(UpdateContext, bIsFocused); 
        });

        RebuildFilteredMessages();
    }

    void FConsoleLogEditorTool::OnDeinitialize(const FUpdateContext& UpdateContext)
    {
        
    }

    void FConsoleLogEditorTool::DrawToolMenu(const FUpdateContext& UpdateContext)
    {
        // Filter Menu
        if (ImGui::BeginMenu(LE_ICON_FILTER " Filter"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

            bool bFilterChanged = false;

            // Log level filters
            ImGui::SeparatorText("Log Levels");
            bFilterChanged |= ImGui::Checkbox("Trace", &Filter.bShowTrace);
            bFilterChanged |= ImGui::Checkbox("Debug", &Filter.bShowDebug);
            bFilterChanged |= ImGui::Checkbox("Info", &Filter.bShowInfo);
            bFilterChanged |= ImGui::Checkbox("Warning", &Filter.bShowWarning);
            bFilterChanged |= ImGui::Checkbox("Error", &Filter.bShowError);
            bFilterChanged |= ImGui::Checkbox("Critical", &Filter.bShowCritical);

            ImGui::Spacing();

            // Quick filter buttons
            if (ImGui::Button("All", ImVec2(70, 0)))
            {
                Filter.bShowTrace = Filter.bShowDebug = Filter.bShowInfo = 
                Filter.bShowWarning = Filter.bShowError = Filter.bShowCritical = true;
                bFilterChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("None", ImVec2(70, 0)))
            {
                Filter.bShowTrace = Filter.bShowDebug = Filter.bShowInfo = 
                Filter.bShowWarning = Filter.bShowError = Filter.bShowCritical = false;
                bFilterChanged = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Errors Only", ImVec2(70, 0)))
            {
                Filter.bShowTrace = Filter.bShowDebug = Filter.bShowInfo = Filter.bShowWarning = false;
                Filter.bShowError = Filter.bShowCritical = true;
                bFilterChanged = true;
            }

            if (bFilterChanged)
            {
                RebuildFilteredMessages();
            }

            ImGui::PopStyleVar(2);
            ImGui::EndMenu();
        }
        
        // Settings Menu
        if (ImGui::BeginMenu(LE_ICON_COG " Settings"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
            
            ImGui::SeparatorText("Display");
            ImGui::Checkbox("Auto Scroll", &Settings.bAutoScroll);
            ImGui::Checkbox("Color Whole Row", &Settings.bColorWholeRow);
            ImGui::Checkbox("Show Timestamps", &Settings.bShowTimestamps);
            ImGui::Checkbox("Show Logger", &Settings.bShowLogger);
            ImGui::Checkbox("Show Icons", &Settings.bShowIcons);
            ImGui::Checkbox("Word Wrap", &Settings.bWordWrap);

            ImGui::Spacing();
            ImGui::SeparatorText("Performance");
            
            if (ImGui::SliderFloat("Font Scale", &Settings.FontScale, 0.7f, 2.0f, "%.1f"))
            {
                // Font scale changed, might need to update rendering
            }

            ImGui::SliderInt("Max Messages", &Settings.MaxMessageCount, 1000, 50000);

            ImGui::Spacing();
            ImGui::SeparatorText("Actions");
            
            if (ImGui::Button("Clear Console", ImVec2(-1, 0)))
            {
                ClearConsole();
            }

            if (ImGui::Button("Export Logs", ImVec2(-1, 0)))
            {
                // Open file dialog or export to default location
                ExportLogs("console_log.txt");
            }

            ImGui::PopStyleVar(2);
            ImGui::EndMenu();
        }

        // Stats in menu bar
        ImGui::Separator();
        ImGui::Text("Messages: %zu / %zu", FilteredMessageCount, PreviousMessageSize);
    }

    void FConsoleLogEditorTool::DrawLogWindow(const FUpdateContext& UpdateContext, bool bIsFocused)
    {
        const ImGuiStyle& Style = ImGui::GetStyle();
        const float InputHeight = ImGui::GetFrameHeightWithSpacing() * 1.2f;
        
        // Search bar
        {
            ImGui::SetNextItemWidth(-1);
            char SearchBuffer[256];
            strncpy(SearchBuffer, Filter.SearchFilter.c_str(), sizeof(SearchBuffer));
            SearchBuffer[sizeof(SearchBuffer) - 1] = '\0';

            if (ImGui::InputTextWithHint("##Search", LE_ICON_MAGNIFY " Search...", 
                SearchBuffer, sizeof(SearchBuffer)))
            {
                Filter.SearchFilter = SearchBuffer;
                RebuildFilteredMessages();
            }
        }

        ImGui::Spacing();

        // Main log area
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 0.95f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        
        const float LogHeight = ImGui::GetContentRegionAvail().y - InputHeight;
        ImGui::BeginChild("##LogMessages", ImVec2(0, LogHeight), true, 
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        
        // Check for new messages
        const TFixedVector<FConsoleMessage, 10000>& Messages = Logging::GetConsoleLogs();
        SIZE_T NewMessageSize = Messages.size();
        
        if (NewMessageSize > PreviousMessageSize)
        {
            bNeedsScrollToBottom = Settings.bAutoScroll;
            RebuildFilteredMessages();
        }
        PreviousMessageSize = NewMessageSize;

        // Font scaling
        if (Settings.FontScale != 1.0f)
        {
            ImGui::PushFontSize(Settings.FontScale);
        }

        // Render log table
        ImGuiTableFlags TableFlags = 
            ImGuiTableFlags_Resizable | 
            ImGuiTableFlags_RowBg | 
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY;

        if (Settings.bShowTimestamps || Settings.bShowLogger)
        {
            TableFlags |= ImGuiTableFlags_Hideable;
        }

        int ColumnCount = 1; // Message column always visible
        if (Settings.bShowTimestamps)
        {
            ColumnCount++;
        }
        if (Settings.bShowLogger)
        {
            ColumnCount++;
        }

        if (ImGui::BeginTable("##LogTable", ColumnCount, TableFlags))
        {
            // Setup columns
            int ColIndex = 0;
            if (Settings.bShowTimestamps)
            {
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 80.0f * Settings.FontScale);
            }
            if (Settings.bShowLogger)
            {
                ImGui::TableSetupColumn("Logger", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 100.0f * Settings.FontScale);
            }
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide);

            ImGui::TableHeadersRow();

            for (SIZE_T i = 0; i < Messages.size(); ++i)
            {
                const FConsoleMessage& Message = Messages[i];

                // Apply filter
                if (!Filter.PassesFilter(FConsoleEntry(Message.Message, Message.Time, Message.LoggerName, Message.Level)))
                {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::PushID((int)i);

                const ImVec4 Color = GetColorForLevel(Message.Level);
                const char* Icon = GetLevelIcon(Message.Level);
                const bool bIsError = (Message.Level == spdlog::level::err || Message.Level == spdlog::level::critical);

                // Optional row highlighting for errors
                if (bIsError && !Settings.bColorWholeRow)
                {
                    ImU32 ErrorBgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.1f, 0.1f, 0.3f));
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ErrorBgColor);
                }

                // Timestamp column
                if (Settings.bShowTimestamps)
                {
                    ImGui::TableSetColumnIndex(0);
                    if (Settings.bColorWholeRow)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, Color);
                    }
                    ImGui::TextUnformatted(Message.Time.c_str());
                    if (Settings.bColorWholeRow)
                    {
                        ImGui::PopStyleColor();
                    }
                }

                // Logger column
                if (Settings.bShowLogger)
                {
                    ImGui::TableSetColumnIndex(Settings.bShowTimestamps ? 1 : 0);
                    if (Settings.bColorWholeRow)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, Color);
                    }
                    ImGui::TextUnformatted(Message.LoggerName.c_str());
                    if (Settings.bColorWholeRow)
                    {
                        ImGui::PopStyleColor();
                    }
                }

                // Message column
                ImGui::TableSetColumnIndex(ColumnCount - 1);
                
                if (Settings.bColorWholeRow)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Color);
                }
                
                if (Settings.bShowIcons)
                {
                    ImGui::TextColored(Color, "%s", Icon);
                    ImGui::SameLine(0, 4);
                }

                if (Settings.bWordWrap)
                {
                    ImGui::PushTextWrapPos(0.0f);
                    if (!Settings.bColorWholeRow)
                    {
                        ImGui::TextColored(Color, "%s", Message.Message.c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted(Message.Message.c_str());
                    }
                    ImGui::PopTextWrapPos();
                }
                else
                {
                    if (!Settings.bColorWholeRow)
                    {
                        ImGui::TextColored(Color, "%s", Message.Message.c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted(Message.Message.c_str());
                    }
                }

                if (Settings.bColorWholeRow)
                {
                    ImGui::PopStyleColor();
                }

                // Context menu per message
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Copy Message"))
                    {
                        ImGui::SetClipboardText(Message.Message.c_str());
                    }
                    if (ImGui::MenuItem("Copy Line"))
                    {
                        FString FullLine = Message.Time + " | " + Message.LoggerName + " | " + Message.Message;
                        ImGui::SetClipboardText(FullLine.c_str());
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            // Auto scroll
            if (bNeedsScrollToBottom)
            {
                ImGui::SetScrollHereY(1.0f);
                bNeedsScrollToBottom = false;
            }
            
            ImGui::EndTable();
        }

        if (Settings.FontScale != 1.0f)
        {
            ImGui::PopFontSize();
        }
    
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    
        // Command input
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        
        char InputBuffer[512];
        strncpy(InputBuffer, CurrentCommand.c_str(), sizeof(InputBuffer));
        InputBuffer[sizeof(InputBuffer) - 1] = '\0';

        ImGuiInputTextFlags InputFlags = 
            ImGuiInputTextFlags_EnterReturnsTrue | 
            ImGuiInputTextFlags_EscapeClearsAll |
            ImGuiInputTextFlags_CallbackAlways;

        auto InputCallback = [](ImGuiInputTextCallbackData* data) -> int
        {
            FConsoleLogEditorTool* Console = static_cast<FConsoleLogEditorTool*>(data->UserData);
            Console->CurrentCommand = FString(data->Buf, data->BufTextLen);
            Console->UpdateAutoComplete(Console->CurrentCommand);
            return 0;
        };

        bool bExecuteCommand = ImGui::InputTextWithHint("##CommandInput", "> Enter command or console variable...", InputBuffer, sizeof(InputBuffer), InputFlags, InputCallback, this);

        CurrentCommand = InputBuffer;

        //ImGui::SameLine();
        //if (ImGui::Button(LE_ICON_HISTORY " History"))
        //{
        //    bShowHistory = !bShowHistory;
        //}

        if (bExecuteCommand && !CurrentCommand.empty())
        {
            ProcessCommand(CurrentCommand);
            AddCommandToHistory(CurrentCommand);
            CurrentCommand.clear();
            bNeedsScrollToBottom = true;
            bShowAutoComplete = false;
            //ImGui::SetKeyboardFocusHere(-1);
        }

        bool bInputFocused = ImGui::IsItemFocused();
        if (bInputFocused && bShowAutoComplete && !AutoCompleteCandidates.empty())
        {
            // Tab to accept auto-complete
            if (ImGui::IsKeyPressed(ImGuiKey_Tab))
            {
                if (AutoCompleteSelectedIndex >= 0 && AutoCompleteSelectedIndex < (int32)AutoCompleteCandidates.size())
                {
                    CurrentCommand = AutoCompleteCandidates[AutoCompleteSelectedIndex].Name;
                    bShowAutoComplete = false;
                }
            }
            // Navigate auto-complete with arrow keys
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                AutoCompleteSelectedIndex = (AutoCompleteSelectedIndex + 1) % AutoCompleteCandidates.size();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                AutoCompleteSelectedIndex--;
                if (AutoCompleteSelectedIndex < 0)
                {
                    AutoCompleteSelectedIndex = AutoCompleteCandidates.size() - 1;
                }
            }
        }
        // History navigation (only when auto-complete is not shown)
        else if (bInputFocused && !bShowAutoComplete)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                NavigateHistory(-1);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                NavigateHistory(1);
            }
        }

        // Ctrl+Space to show history
        if (bInputFocused && ImGui::IsKeyPressed(ImGuiKey_Space) && (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper))
        {
            bShowHistory = !bShowHistory;
        }

        // Draw auto-complete popup
        if (bShowAutoComplete && !AutoCompleteCandidates.empty())
        {
            DrawAutoCompletePopup();
        }

        // Draw history popup
        if (bShowHistory)
        {
            DrawHistoryPopup();
        }

        // Autofocus input when window is focused
        if (bIsFocused && !ImGui::IsAnyItemActive())
        {
            //ImGui::SetKeyboardFocusHere(-1);
        }
    }

    ImVec4 FConsoleLogEditorTool::GetColorForLevel(spdlog::level::level_enum Level)
    {
        switch (Level)
        {
            case spdlog::level::err:      return ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
            case spdlog::level::warn:     return ImVec4(1.00f, 0.70f, 0.20f, 1.0f);
            case spdlog::level::info:     return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
            case spdlog::level::debug:    return ImVec4(0.45f, 0.80f, 1.00f, 1.0f);
            case spdlog::level::trace:    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            case spdlog::level::critical: return ImVec4(1.00f, 0.10f, 0.10f, 1.0f);
            default:                      return ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
        }
    }


    void FConsoleLogEditorTool::ProcessCommand(const FString& Command)
    {
        LOG_INFO("> {}", Command);

        size_t Index = Command.find_first_of(' ');
        if (Index != FString::npos)
        {
            FString VariableName = Command.substr(0, Index);
            FString ValueString = Command.substr(Index + 1);


            if (FConsoleRegistry::Get().Find(VariableName))
            {
                if (FConsoleRegistry::Get().SetValueFromString(VariableName, ValueString))
                {
                    LOG_INFO("{} New Value: {}", VariableName, ValueString);
                }
                else
                {
                    LOG_WARN("Failed to execute command {}", Command);
                }
            }
        }
    }

    void FConsoleLogEditorTool::AddCommandToHistory(const FString& Command)
    {
        if (!CommandHistory.empty() && CommandHistory.back() == Command)
        {
            HistoryIndex = CommandHistory.size();
            return;
        }

        CommandHistory.push_back(Command);

        constexpr SIZE_T MaxHistory = 100;
        if (CommandHistory.size() > MaxHistory)
        {
            CommandHistory.pop_front();
        }
        
        HistoryIndex = CommandHistory.size();
    }

    void FConsoleLogEditorTool::NavigateHistory(int32 Direction)
    {
        if (CommandHistory.empty())
        {
            return;
        }

        if (Direction < 0)
        {
            if (HistoryIndex > 0)
            {
                HistoryIndex--;
                CurrentCommand = CommandHistory[HistoryIndex];
            }
        }
        else if (Direction > 0)
        {
            if (HistoryIndex < CommandHistory.size() - 1)
            {
                HistoryIndex++;
                CurrentCommand = CommandHistory[HistoryIndex];
            }
            else if (HistoryIndex == CommandHistory.size() - 1)
            {
                HistoryIndex++;
                CurrentCommand.clear();
            }
        }
    }

    void FConsoleLogEditorTool::ClearConsole()
    {
        PreviousMessageSize = 0;
        FilteredMessageCount = 0;
        FilteredMessages.clear();
        bNeedsScrollToBottom = false;
    }

    void FConsoleLogEditorTool::ExportLogs(const FString& FilePath)
    {
        LOG_INFO("Exporting logs to: {}", FilePath);
    }

    void FConsoleLogEditorTool::RebuildFilteredMessages()
    {
        FilteredMessages.clear();
        FilteredMessageCount = 0;

        const TFixedVector<FConsoleMessage, 10000>& Messages = Logging::GetConsoleLogs();
        
        for (SIZE_T i = 0; i < Messages.size(); ++i)
        {
            const FConsoleMessage& Message = Messages[i];
            FConsoleEntry Entry(Message.Message, Message.Time, Message.LoggerName, Message.Level);
            
            if (Filter.PassesFilter(Entry))
            {
                FilteredMessageCount++;
            }
        }
    }

    void FConsoleLogEditorTool::UpdateAutoComplete(const FString& CurrentInput)
    {
        AutoCompleteCandidates.clear();
        AutoCompleteSelectedIndex = 0;

        // Only show auto-complete if there's input
        if (CurrentInput.empty())
        {
            bShowAutoComplete = false;
            return;
        }
        
        const FConsoleRegistry::FConsoleContainer& Container = FConsoleRegistry::Get().GetAll();

        // Calculate match scores and filter
        for (const auto& [Name, Var] : Container)
        {
            float Score = CalculateMatchScore(Name, CurrentInput);
            if (Score > 0.0f)
            {
                TOptional<FString> Value = FConsoleRegistry::Get().GetValueAsString(Name);

                if (Value.has_value())
                {
                    AutoCompleteCandidates.emplace_back(FAutoCompleteCandidate(Name, Var.Hint, Value.value(), Score));
                }
            }
        }

        // Sort by match score (highest first)
        eastl::sort(AutoCompleteCandidates.begin(), AutoCompleteCandidates.end(), [](const FAutoCompleteCandidate& A, const FAutoCompleteCandidate& B)
        {
            return A.MatchScore > B.MatchScore;
        });

        // Limit to top 10 matches
        if (AutoCompleteCandidates.size() > 10)
        {
            AutoCompleteCandidates.resize(10);
        }

        bShowAutoComplete = !AutoCompleteCandidates.empty();
    }

    void FConsoleLogEditorTool::DrawAutoCompletePopup()
    {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y - 5), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 0));

        ImGuiWindowFlags PopupFlags = 
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));

        if (ImGui::Begin("##AutoComplete", nullptr, PopupFlags))
        {
            // Header
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Auto-Complete (%zu matches)", AutoCompleteCandidates.size());
            ImGui::Separator();

            // Render candidates
            for (int32 i = 0; i < (int32)AutoCompleteCandidates.size(); ++i)
            {
                const FAutoCompleteCandidate& Candidate = AutoCompleteCandidates[i];
                
                bool bIsSelected = (i == AutoCompleteSelectedIndex);
                
                if (bIsSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
                }

                ImGui::PushID(i);
                
                if (ImGui::Selectable("##Candidate", bIsSelected, 0, ImVec2(0, 0)))
                {
                    CurrentCommand = Candidate.Name;
                    bShowAutoComplete = false;
                }

                ImGui::SameLine(0, 4);

                // Command name
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%s", Candidate.Name.data());
                
                // Current value
                if (!Candidate.CurrentValue.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "= %s", Candidate.CurrentValue.c_str());
                }

                // Description on next line if available
                if (!Candidate.Description.empty())
                {
                    ImGui::Indent(20.0f);
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", Candidate.Description.data());
                    ImGui::Unindent(20.0f);
                }

                ImGui::PopID();

                if (bIsSelected)
                {
                    ImGui::PopStyleColor();
                }
            }

            // Footer with hints
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), LE_ICON_ARROW_UP_DOWN " Navigate  " LE_ICON_KEYBOARD_TAB " Accept  " LE_ICON_KEYBOARD_ESC " Cancel");
        }
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void FConsoleLogEditorTool::DrawHistoryPopup()
    {
               ImGui::SetNextWindowPos(ImVec2(
            ImGui::GetItemRectMin().x,
            ImGui::GetItemRectMin().y - 5
        ), ImGuiCond_Always, ImVec2(0.0f, 1.0f));

        ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 300));

        ImGuiWindowFlags PopupFlags = 
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 3));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));

        if (ImGui::Begin("##CommandHistory", nullptr, PopupFlags))
        {
            // Header
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
                LE_ICON_HISTORY " Command History (%zu)", CommandHistory.size());
            
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear"))
            {
                CommandHistory.clear();
                HistoryIndex = 0;
            }

            ImGui::Separator();

            // Scrollable area
            ImGui::BeginChild("##HistoryList", ImVec2(0, 0), false);

            if (CommandHistory.empty())
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No command history");
            }
            else
            {
                // Display in reverse order (newest first)
                for (int32 i = (int32)CommandHistory.size() - 1; i >= 0; --i)
                {
                    const FString& Cmd = CommandHistory[i];
                    
                    ImGui::PushID(i);
                    
                    bool bIsSelected = (i == (int32)HistoryIndex);
                    
                    if (ImGui::Selectable(Cmd.c_str(), bIsSelected))
                    {
                        CurrentCommand = Cmd;
                        bShowHistory = false;
                        //ImGui::SetKeyboardFocusHere(-1); // Focus back to input
                    }

                    // Context menu for individual history items
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Execute"))
                        {
                            CurrentCommand = Cmd;
                            ProcessCommand(CurrentCommand);
                            bShowHistory = false;
                        }
                        if (ImGui::MenuItem("Copy"))
                        {
                            ImGui::SetClipboardText(Cmd.c_str());
                        }
                        if (ImGui::MenuItem("Delete"))
                        {
                            CommandHistory.erase(CommandHistory.begin() + i);
                            HistoryIndex = std::min(HistoryIndex, CommandHistory.size());
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndChild();
        }
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        // Click outside to close
        if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        {
            bShowHistory = false;
        }
    }

    float FConsoleLogEditorTool::CalculateMatchScore(FStringView Candidate, const FString& Input)
    {
        if (Candidate.empty() || Input.empty())
        {
            return 0.0f;
        }

        FString LowerCandidate = FString(Candidate.data());
        FString LowerInput = Input;

        if (LowerCandidate.find(LowerInput) == 0)
        {
            return 1.0f;
        }

        size_t Pos = LowerCandidate.find(LowerInput);
        if (Pos != FString::npos)
        {
            return 0.7f - (float)Pos / (float)LowerCandidate.size() * 0.2f;
        }

        size_t CandidatePos = 0;
        size_t MatchedChars = 0;
        
        for (size_t i = 0; i < Input.size() && CandidatePos < Candidate.size(); ++i)
        {
            char InputChar = tolower(Input[i]);
            while (CandidatePos < Candidate.size())
            {
                if (tolower(Candidate[CandidatePos]) == InputChar)
                {
                    MatchedChars++;
                    CandidatePos++;
                    break;
                }
                CandidatePos++;
            }
        }

        if (MatchedChars == Input.size())
        {
            return 0.5f * (float)MatchedChars / (float)Candidate.size();
        }

        return 0.0f;
    }
    
    const char* FConsoleLogEditorTool::GetLevelIcon(spdlog::level::level_enum Level) const
    {
        if (!Settings.bShowIcons)
        {
            return "";
        }

        switch (Level)
        {
            case spdlog::level::err:      return LE_ICON_ALERT_CIRCLE;
            case spdlog::level::warn:     return LE_ICON_ALERT;
            case spdlog::level::info:     return LE_ICON_INFORMATION;
            case spdlog::level::debug:    return LE_ICON_BUG;
            case spdlog::level::trace:    return LE_ICON_DOTS_HORIZONTAL;
            case spdlog::level::critical: return LE_ICON_ALERT_OCTAGON;
            default:                      return LE_ICON_INFORMATION;
        }
    }

    const char* FConsoleLogEditorTool::GetLevelLabel(spdlog::level::level_enum Level) const
    {
        switch (Level)
        {
            case spdlog::level::err:      return "Error";
            case spdlog::level::warn:     return "Warning";
            case spdlog::level::info:     return "Info";
            case spdlog::level::debug:    return "Debug";
            case spdlog::level::trace:    return "Trace";
            case spdlog::level::critical: return "Critical";
            default:                      return "Unknown";
        }
    }
}
