#pragma once
#include "EditorTool.h"

namespace Lumina
{
    enum class EConsoleMessageType : uint8
    {
        Log,
        Command,
        CommandResult,
        Warning,
        Error
    };

    struct FConsoleEntry
    {
        FString Text;
        FString Time;
        FString Logger;
        spdlog::level::level_enum Level;
        EConsoleMessageType Type;
        ImVec4 Color;
        
        FConsoleEntry(const FString& InText, const FString& InTime, const FString& InLogger, 
                      spdlog::level::level_enum InLevel, EConsoleMessageType InType = EConsoleMessageType::Log)
            : Text(InText)
            , Time(InTime)
            , Logger(InLogger)
            , Level(InLevel)
            , Type(InType)
        {
            UpdateColor();
        }

        void UpdateColor()
        {
            switch (Level)
            {
                case spdlog::level::err:      Color = ImVec4(0.95f, 0.25f, 0.25f, 1.0f); break;
                case spdlog::level::warn:     Color = ImVec4(1.00f, 0.70f, 0.20f, 1.0f); break;
                case spdlog::level::info:     Color = ImVec4(0.90f, 0.90f, 0.90f, 1.0f); break;
                case spdlog::level::debug:    Color = ImVec4(0.45f, 0.80f, 1.00f, 1.0f); break;
                case spdlog::level::trace:    Color = ImVec4(0.55f, 0.55f, 0.55f, 1.0f); break;
                case spdlog::level::critical: Color = ImVec4(1.00f, 0.10f, 0.10f, 1.0f); break;
                default:                      Color = ImVec4(0.80f, 0.80f, 0.80f, 1.0f); break;
            }

            if (Type == EConsoleMessageType::Command)
            {
                Color = ImVec4(0.60f, 1.00f, 0.60f, 1.0f);
            }
            else if (Type == EConsoleMessageType::CommandResult)
            {
                Color = ImVec4(0.70f, 0.70f, 1.00f, 1.0f);
            }
        }
    };

    struct FConsoleFilter
    {
        bool bShowTrace = true;
        bool bShowDebug = true;
        bool bShowInfo = true;
        bool bShowWarning = true;
        bool bShowError = true;
        bool bShowCritical = true;
        FString SearchFilter;

        bool PassesFilter(const FConsoleEntry& Entry) const
        {
            // Level filter
            switch (Entry.Level)
            {
                case spdlog::level::trace:    if (!bShowTrace) return false; break;
                case spdlog::level::debug:    if (!bShowDebug) return false; break;
                case spdlog::level::info:     if (!bShowInfo) return false; break;
                case spdlog::level::warn:     if (!bShowWarning) return false; break;
                case spdlog::level::err:      if (!bShowError) return false; break;
                case spdlog::level::critical: if (!bShowCritical) return false; break;
                default: break;
            }

            // Text search filter
            if (!SearchFilter.empty())
            {
                FString LowerText = Entry.Text;
                FString LowerFilter = SearchFilter;
                // Convert to lowercase for case-insensitive search
                // You'll need to implement this based on your string class
                if (LowerText.find(LowerFilter) == FString::npos)
                {
                    return false;
                }
            }

            return true;
        }
    };

    struct FAutoCompleteCandidate
    {
        FStringView Name;
        FStringView Description;
        FString CurrentValue;
        float MatchScore;

        FAutoCompleteCandidate() = default;
        FAutoCompleteCandidate(FStringView InName, FStringView InDesc = "", const FString& InValue = "", float InScore = 0.0f)
            : Name(InName)
            , Description(InDesc)
            , CurrentValue(InValue)
            , MatchScore(InScore)
        {}
    };
    
    class FConsoleLogEditorTool : public FEditorTool
    {
    public:
        LUMINA_SINGLETON_EDITOR_TOOL(FConsoleLogEditorTool)
    
        FConsoleLogEditorTool(IEditorToolContext* Context)
            : FEditorTool(Context, "Console", nullptr)
            , HistoryIndex(0)
            , bNeedsScrollToBottom(false)
            , FilteredMessageCount(0)
            , bShowAutoComplete(false)
            , AutoCompleteSelectedIndex(0)
            , bShowHistory(false)
        {}

        bool IsSingleWindowTool() const override { return true; }
        const char* GetTitlebarIcon() const override { return LE_ICON_FORMAT_LIST_BULLETED_TYPE; }
        
        void OnInitialize() override;
        void OnDeinitialize(const FUpdateContext& UpdateContext) override;
        void DrawToolMenu(const FUpdateContext& UpdateContext) override;
        void DrawLogWindow(const FUpdateContext& UpdateContext, bool bIsFocused);

    private:
        void ProcessCommand(const FString& Command);
        void AddCommandToHistory(const FString& Command);
        void NavigateHistory(int32 Direction);
        void ClearConsole();
        void ExportLogs(const FString& FilePath);
        void RebuildFilteredMessages();
        void UpdateAutoComplete(const FString& CurrentInput);
        void DrawAutoCompletePopup();
        void DrawHistoryPopup();
        float CalculateMatchScore(FStringView Candidate, const FString& Input);
        
        const char* GetLevelIcon(spdlog::level::level_enum Level) const;
        const char* GetLevelLabel(spdlog::level::level_enum Level) const;
        static ImVec4 GetColorForLevel(spdlog::level::level_enum Level);

        // State
        SIZE_T PreviousMessageSize = 0;
        TDeque<FString> CommandHistory;
        FString CurrentCommand;
        uint64 HistoryIndex;
        bool bNeedsScrollToBottom;
        uint32 FilteredMessageCount;

        // Settings
        struct FConsoleSettings
        {
            bool bAutoScroll = true;
            bool bColorWholeRow = false;
            bool bShowTimestamps = true;
            bool bShowLogger = true;
            bool bShowIcons = true;
            bool bWordWrap = true;
            float FontScale = 1.0f;
            int32 MaxMessageCount = 10000;
        } Settings;

        FConsoleFilter Filter;
        
        // Cached filtered messages for performance
        TVector<const FConsoleEntry*> FilteredMessages;

        // Auto-complete state
        bool bShowAutoComplete;
        int32 AutoCompleteSelectedIndex;
        TVector<FAutoCompleteCandidate> AutoCompleteCandidates;

        // History popup state
        bool bShowHistory;
    };
}