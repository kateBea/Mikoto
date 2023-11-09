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
#include "Common/Random.hh"
#include "Common/Types.hh"
#include "HierarchyPanel.hh"
#include "Panel.hh"

namespace Mikoto {
    class InspectorPanel : public Panel {
    public:
        explicit InspectorPanel();
        auto operator=(InspectorPanel&& other) -> InspectorPanel& = default;

        auto OnUpdate(float timeStep) -> void override;

        ~InspectorPanel() override = default;

    private:
        auto DrawComponents() -> void;
        auto MaterialComponentEditor(MaterialComponent& material) -> void;

        auto OpenMaterialEditor() -> void;

    private:
        static constexpr Size_T REQUIRED_IDS{ 4 };
        std::vector<Random::GUID::UUID> m_Guids{};

        bool m_OpenMaterialEditor{};
        std::shared_ptr<Material> m_TargetMaterialForMaterialEditor{ nullptr };

        std::shared_ptr<Texture2D> m_EmptyTexturePlaceHolder{};
        std::shared_ptr<Texture2D> m_EmptyMaterialPreviewPlaceHolder{};
    };
}


#endif // MIKOTO_INSPECTOR_PANEL_HH
