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

#ifndef MIKOTO_PROJECT_PANEL_HH
#define MIKOTO_PROJECT_PANEL_HH

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct ProjectPanelCreateInfo {
        EditorState* mState{};
    };

    class ProjectPanel final : public Panel {
    public:
        explicit ProjectPanel(const ProjectPanelCreateInfo& info);

        auto OnRender( float timeStep ) -> void override;

    private:
        EditorState* mEditorState{};
    };
}// namespace mikoto::editor

#endif//MIKOTO_PROJECT_PANEL_HH
