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
#include <Utility/Types.hh>
#include <Utility/Random.hh>
#include <Editor/Panel.hh>
#include <Editor/HierarchyPanel.hh>

namespace Mikoto {
    class InspectorPanel : public Panel {
    public:
        explicit InspectorPanel();
        auto operator=(InspectorPanel&& other) -> InspectorPanel& = default;

        auto OnUpdate() -> void override;

        ~InspectorPanel() override = default;

    private:
        static constexpr Size_T REQUIRED_IDS{ 4 };
        std::vector<Random::GUID::UUID> m_Guids{};
    };
}


#endif // MIKOTO_INSPECTOR_PANEL_HH
