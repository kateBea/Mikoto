//
// Created by kate on 11/13/23.
//

#ifndef MIKOTO_ASSETS_MANAGER_PANEL_HH
#define MIKOTO_ASSETS_MANAGER_PANEL_HH

#include <Panels/Panel.hh>

namespace Mikoto {

    struct AssetsPanelDescription {

    };

    /**
     * @class AssetsPanel
     * @brief A panel for managing assets in the editor.
     *
     * This class provides functionality to manage and display assets in the editor.
     * It inherits from the Panel class and overrides the OnUpdate method.
     */
    class AssetsPanel final : public Panel {
    public:
        explicit AssetsPanel( const AssetsPanelDescription& description );

        auto OnUpdate( float timeStep ) -> void override;
    };
}


#endif//MIKOTO_ASSETS_MANAGER_PANEL_HH
