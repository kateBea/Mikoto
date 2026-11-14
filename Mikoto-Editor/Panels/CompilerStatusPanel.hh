//    Copyright 2026 ã‚±ã‚¤ãƒˆ
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

#ifndef MIKOTO_PANELS_COMPILER_STATUS_PANEL_HH
#define MIKOTO_PANELS_COMPILER_STATUS_PANEL_HH

#include <imgui.h>

#include <Scripting/ScriptBuildTracker.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;
    struct CompilerStatusPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class CompilerStatusPanel final : public Panel {
    public:
        explicit CompilerStatusPanel( const CompilerStatusPanelCreateInfo& info );
        auto OnRender( core::f32 timeStep ) -> void override;
        auto UpdateBuilds( const filesystem::Path& assets, const filesystem::Path& project = {} ) -> void;

    private:
        auto DrawBuildPopup() -> void;
        scripting::ScriptBuildTracker mBuildTracker{};
        core::u64 mBuildSequence{};
        core::u64 mScriptCompilationRevision{};
        double mBuildVisibleUntil{};
        EditorState* mState{};
        ImGuiTextFilter mFilter{};
        bool mErrorsOnly{};
        bool mOpenFailed{};
    };
}

#endif// MIKOTO_PANELS_COMPILER_STATUS_PANEL_HH

