//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_ASSETS_MANAGER_PANEL_HH
#define MIKOTO_ASSETS_MANAGER_PANEL_HH

#include <imgui.h>

#include <Common/Common.hh>
#include <Panels/Panel.hh>

namespace Mikoto {
    struct EditorState;

    struct AssetsPanelDescription {
        EditorState* State{};
    };

    /**
     * @class AssetsPanel
     * @brief A panel for managing assets currently in use by the editor.
     */
    class AssetsPanel final : public Panel {
    public:
        explicit AssetsPanel( const AssetsPanelDescription& description );

        auto OnUpdate( float timeStep ) -> void override;

    private:
        auto UpdateViewport() -> void;
        auto CreateImguiTextureID() -> void;

        MKT_NODISCARD auto IsDisplayTextureValid() const -> bool;

    private:
        float m_ViewportWidth{};
        float m_ViewportHeight{};

        EditorState* m_EditorState{};
        ImTextureID m_DisplayTargetImGuiID{};
    };
}


#endif//MIKOTO_ASSETS_MANAGER_PANEL_HH
