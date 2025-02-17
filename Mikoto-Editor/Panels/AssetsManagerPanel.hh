//
// Created by kate on 11/13/23.
//

#ifndef MIKOTO_ASSETS_MANAGER_PANEL_HH
#define MIKOTO_ASSETS_MANAGER_PANEL_HH

#include <Panels/Panel.hh>

namespace Mikoto {
    // will display the currently loaded assets
    class AssetsManagerPanel final : public Panel {
    public:
        auto OnUpdate( float timeStep ) -> void override;
    };
}


#endif//MIKOTO_ASSETS_MANAGER_PANEL_HH
