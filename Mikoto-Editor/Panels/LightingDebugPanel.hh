//
// Created by zanet on 1/6/2026.
//

#ifndef MIKOTO_LIGHTINGDEBUGPANEL_HH
#define MIKOTO_LIGHTINGDEBUGPANEL_HH

#include <memory>

// Project Headers
#include <Common/Common.hh>
#include <Panels/Panel.hh>

namespace Mikoto {

    struct EditorState;

    struct LightingDebugPanelCreateInfo {
        EditorState *State{};
    };

    class LightingDebugPanel final : public Panel {
    public:
        explicit LightingDebugPanel(const LightingDebugPanelCreateInfo& info);

        auto OnUpdate(float timeStep) -> void override;

    private:
        auto DisplaySelectedLightProperties() const -> void;

    private:
        EditorState *m_EditorState{};

    };
}


#endif //MIKOTO_LIGHTINGDEBUGPANEL_HH