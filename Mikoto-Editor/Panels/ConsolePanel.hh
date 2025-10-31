//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_CONSOLE_PANEL_HH
#define MIKOTO_CONSOLE_PANEL_HH

#include <imgui.h>

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
