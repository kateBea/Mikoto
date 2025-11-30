//
// Created by kate on 11/30/25.
//

#ifndef MIKOTO_PASSVISUALIZERPANEL_HH
#define MIKOTO_PASSVISUALIZERPANEL_HH

#include <string>

#include <Panels/Panel.hh>

namespace Mikoto {

    struct EditorState;

    struct PassVisualizerDescription {
        EditorState* State{};
    };

    class PassVisualizerPanel final : public Panel {
    public:

        explicit PassVisualizerPanel( const PassVisualizerDescription& description );

        auto OnUpdate( float timeStep ) -> void override;

    private:
        std::string m_ActivePassName{};
        EditorState* m_EditorState{};
    };

}

#endif//MIKOTO_PASSVISUALIZERPANEL_HH
