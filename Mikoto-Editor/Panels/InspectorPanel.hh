/**
 * InspectorPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_INSPECTOR_PANEL_HH
#define MIKOTO_INSPECTOR_PANEL_HH

// C++ Standard Library
#include <memory>
#include <vector>

// Project Headers
#include <Common/Random.hh>
#include <Common/Types.hh>
#include <Panels/HierarchyPanel.hh>

#include "Panel.hh"

namespace Mikoto {
    class InspectorPanel : public Panel {
    public:
        explicit InspectorPanel();
        auto operator=( InspectorPanel&& other ) -> InspectorPanel& = default;

        auto OnUpdate( float timeStep ) -> void override;

        ~InspectorPanel() override = default;

    private:
        static auto DrawComponents( Entity& entity ) -> void;

        auto OpenMaterialEditor() -> void;

    private:
        // TODO: move to material editor window/panel
        bool m_OpenMaterialEditor{};
        std::shared_ptr<Material> m_TargetMaterialForMaterialEditor{};

        std::shared_ptr<Texture2D> m_EmptyTexturePlaceHolder{};
    };
}

#endif// MIKOTO_INSPECTOR_PANEL_HH
