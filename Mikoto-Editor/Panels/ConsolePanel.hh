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

namespace Mikoto {
    struct EditorState;

    struct ConsolePanelCreateInfo {
        EditorState *State{};
    };

    class ConsolePanel final : public Panel {
    public:
        explicit ConsolePanel(const ConsolePanelCreateInfo& info);
        auto operator=(ConsolePanel&& other) -> ConsolePanel& = default;

        auto OnUpdate(float timeStep) -> void override;

        ~ConsolePanel() override = default;

    private:
        EditorState *m_State{};
        bool m_ScrollToBottom{ false };
    };
}

#endif // MIKOTO_CONSOLE_PANEL_HH
