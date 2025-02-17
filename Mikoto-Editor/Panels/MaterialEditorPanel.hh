//
// Created by kate on 11/13/23.
//

#ifndef MIKOTO_MATERIAL_EDITOR_PANEL_HH
#define MIKOTO_MATERIAL_EDITOR_PANEL_HH

#include <Panels/Panel.hh>

namespace Mikoto {
    class MaterialEditorPanel final : public Panel {
    public:
        auto OnUpdate( float timeStep ) -> void override;


    private:

    };

}

#endif//MIKOTO_MATERIAL_EDITOR_PANEL_HH
