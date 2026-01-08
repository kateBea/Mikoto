//
// Created by zanet on 1/9/2026.
//

#ifndef MIKOTO_SCENE_PROPERTIES_HH
#define MIKOTO_SCENE_PROPERTIES_HH

#include <Common/Common.hh>
#include <Panels/Panel.hh>

namespace Mikoto {
    struct EditorState;

    struct ScenePropertiesPanelCreateInfo {
        EditorState *State{};
    };

    class ScenePropertiesPanel final : public Panel {
    public:
        explicit ScenePropertiesPanel(const ScenePropertiesPanelCreateInfo& info);

        auto OnUpdate(float timeStep) -> void override;

    private:
        EditorState *m_EditorState{};

    };
}



#endif //MIKOTO_SCENE_PROPERTIES_HH