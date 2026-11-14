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

#ifndef MIKOTO_PANELS_SETTINGS_PANEL_HH
#define MIKOTO_PANELS_SETTINGS_PANEL_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct SettingsPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class SettingsPanel final : public Panel {
    public:
        explicit SettingsPanel( const SettingsPanelCreateInfo& data );

        auto OnRender( float timeStep ) -> void override;

    private:
        static auto BeginProperties( core::cstr id ) -> bool;
        static auto Property( core::cstr label ) -> void;
        auto DrawNavigation() -> void;
        auto DrawLens() -> void;
        auto DrawView() -> void;
        auto DrawDisplay() -> void;

    private:
        EditorState* mEditorState{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_SETTINGS_PANEL_HH

