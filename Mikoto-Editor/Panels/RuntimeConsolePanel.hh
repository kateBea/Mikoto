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

#ifndef MIKOTO_CONSOLE_PANEL_HH
#define MIKOTO_CONSOLE_PANEL_HH

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct RuntimeConsolePanelCreateInfo {
        EditorState *mState{};
    };

    class RuntimeConsolePanel final : public Panel {
    public:
        explicit RuntimeConsolePanel(const RuntimeConsolePanelCreateInfo& info);

        auto OnUpdate(float timeStep) -> void override;

        ~RuntimeConsolePanel() override = default;

    private:
        EditorState *mState{};
        bool mScrollToBottom{ false };
    };
}

#endif // MIKOTO_CONSOLE_PANEL_HH
