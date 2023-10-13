//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_CONSOLE_PANEL_HH
#define MIKOTO_CONSOLE_PANEL_HH

#include <imgui.h>

#include <Editor/Panel.hh>

namespace Mikoto {
    class ConsolePanel : public Panel {
    public:
        explicit ConsolePanel();
        auto operator=(ConsolePanel && other) -> ConsolePanel& = default;

        auto OnUpdate(float timeStep) -> void override;

        ~ConsolePanel() override = default;

    private:
        auto DisplayMessages() -> void;

    private:
        ImGuiTextFilter m_SearchFilter{};

    };
}

#endif // MIKOTO_CONSOLE_PANEL_HH
