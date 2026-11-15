//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_PANELS_RUNTIME_CONSOLE_PANEL_HH
#define MIKOTO_PANELS_RUNTIME_CONSOLE_PANEL_HH

#include <EASTL/array.h>
#include <EASTL/deque.h>
#include <imgui.h>

#include <Core/RuntimeConsole.hh>
#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct RuntimeConsolePanelCreateInfo {
        EditorState *mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class RuntimeConsolePanel final : public Panel {
    public:
        explicit RuntimeConsolePanel(const RuntimeConsolePanelCreateInfo& info);

        auto OnRender(float timeStep) -> void override;
        auto FocusCommandInput() -> void;

        ~RuntimeConsolePanel() override = default;

    private:
        struct DisplayLine {
            core::u64 mEntryID{};
            core::usize mOffset{};
            core::usize mLength{};
        };

        struct MeasuredLine {
            core::usize mOffset{};
            core::usize mLength{};
        };

        struct CachedLog {
            core::ConsoleLogEntry mEntry{};
            eastl::vector<MeasuredLine> mLines{};
            core::f32 mWidth{};
        };

        struct WidthCandidate {
            core::u64 mEntryID{};
            core::f32 mWidth{};
        };

        auto UpdateLogCache( bool rebuild ) -> void;
        auto MeasureLog( CachedLog& log ) -> void;
        auto AppendDisplayLines( const CachedLog& log ) -> void;
        auto DrawLogLine( const CachedLog& log, const DisplayLine& line ) -> void;
        auto CommandInputCallback( ImGuiInputTextCallbackData& data ) -> int;

        EditorState *mState{};

        bool mScrollToBottom{ false };
        bool mAutoScroll{ true };
        bool mWasAtBottom{ true };
        bool mPaused{};

        bool mFocusCommandInput{};

        ImGuiTextFilter mTextFilter{};
        bool mShowInfo{ true };
        bool mShowWarning{ true };
        bool mShowError{ true };
        bool mShowDebug{ true };

        core::u64 mNextLogID{};
        eastl::vector<core::ConsoleLogEntry> mIncomingLogs{};
        eastl::deque<CachedLog> mLogs{};
        eastl::deque<DisplayLine> mDisplayLines{};
        eastl::deque<WidthCandidate> mWidthCandidates{};

        eastl::array<char, 256> mCommandInput{};
        eastl::vector<eastl::string> mCommandHistory{};
        eastl::string mCommandDraft{};
        core::i32 mHistoryPosition{ -1 };

        eastl::array<core::usize, 4> mLevelCounts{};

        ImFont* mLogFont{};
        core::f32 mLogFontSize{};
        core::f32 mContentWidth{};
    };
}

#endif// MIKOTO_PANELS_RUNTIME_CONSOLE_PANEL_HH

