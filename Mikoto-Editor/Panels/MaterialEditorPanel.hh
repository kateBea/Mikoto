//
// Created by kate on 11/13/23.
//

#ifndef MIKOTO_MATERIAL_EDITOR_PANEL_HH
#define MIKOTO_MATERIAL_EDITOR_PANEL_HH

#include <Panels/Panel.hh>

namespace Mikoto {

    struct MaterialEditorPanelDescription {
        // Add any necessary parameters for the material editor panel
    };

    class MaterialEditorPanel final : public Panel {
    public:
        explicit MaterialEditorPanel( const MaterialEditorPanelDescription& description );

        auto OnUpdate( float timeStep ) -> void override;

    private:

    };

}

#endif//MIKOTO_MATERIAL_EDITOR_PANEL_HH
