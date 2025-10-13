//
// Created by kate on 10/13/25.
//

#ifndef UTILITY_HH
#define UTILITY_HH
#include <Common/Common.hh>

namespace Mikoto {
    /**
     * Utility function to make panel names for ImGui windows.
     * @param panelIcon Panel's icon value.
     * @param panelName Name of the panel.
     * @returns The panel's name including the icon.
     * */
    MKT_NODISCARD inline auto MakePanelName(std::string_view panelIcon, std::string_view panelName) -> std::string {
        return fmt::format("{} {}", panelIcon, panelName);
    }

}
#endif //UTILITY_HH
