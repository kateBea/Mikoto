/**
 * InspectorPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_INSPECTOR_PANEL_HH
#define MIKOTO_INSPECTOR_PANEL_HH

// C++ Standard Library
#include <memory>

#include <ankerl/unordered_dense.h>

// Project Headers
#include <Panels/Panel.hh>
#include <Scene/Entity.hh>
#include <Assets/Texture.hh>

namespace Mikoto {
    struct EditorState;

    struct InspectorPanelCreateInfo {
        EditorState* State{};
    };

    class InspectorPanel final : public Panel {
    public:
        explicit InspectorPanel( const InspectorPanelCreateInfo& createInfo );

        auto OnUpdate( float timeStep ) -> void override;

        ~InspectorPanel() override = default;

    private:
        auto DrawComponents( Entity* entity ) const -> void;

    private:
        EditorState* m_State{};
    };
}// namespace Mikoto

#endif// MIKOTO_INSPECTOR_PANEL_HH
