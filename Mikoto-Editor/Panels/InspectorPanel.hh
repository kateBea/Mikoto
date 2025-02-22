/**
 * InspectorPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_INSPECTOR_PANEL_HH
#define MIKOTO_INSPECTOR_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Panels/Panel.hh>
#include <Scene/Scene/Entity.hh>

namespace Mikoto {
    struct InspectorPanelCreateInfo {
        Scene* TargetScene{ nullptr };
        std::function<Entity*()> GetActiveEntityCallback{};
        std::function<void(Entity*)> SetActiveEntityCallback{};
    };

    class InspectorPanel final : public Panel {
    public:
        explicit InspectorPanel(const InspectorPanelCreateInfo& createInfo);

        auto OnUpdate( float timeStep ) -> void override;

        ~InspectorPanel() override = default;

    private:
        auto DrawComponents( Entity& entity ) const -> void;

    private:
        Scene* m_TargetScene{ nullptr };

        std::function<Entity*()> m_GetActiveEntityCallback{};
        std::function<void(Entity*)> m_SetActiveEntityCallback{};
    };
}

#endif// MIKOTO_INSPECTOR_PANEL_HH
