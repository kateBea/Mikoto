//
// Created by zanet on 1/6/2026.
//


#include <array>
#include <string_view>
#include <typeinfo>

// Third-Party Libraries
#include "imgui.h"

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Passes/IBLPasses.hh>
#include <Renderer/Passes/ClusteredShading.hh>
#include <Renderer/Passes/ClusteredShading.hh>

#include <Panels/LightingDebugPanel.hh>

#include <Scene/Component.hh>

namespace Mikoto {

    LightingDebugPanel::LightingDebugPanel( const LightingDebugPanelCreateInfo &info )
        : Panel{ "Lighting Debug" }, m_EditorState{ info.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_TABLE_CHART, m_PanelName );
    }

    auto LightingDebugPanel::OnUpdate( float timeStep ) -> void {
        // Display info about clustered forward
        // this is mainly a debug pass
        if (!m_PanelIsVisible) {
            return;
        }

        constexpr ImGuiWindowFlags flags{ ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize };
        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, flags);

        DisplaySelectedLightProperties();

        ImGui::End();
    }

    auto LightingDebugPanel::DisplaySelectedLightProperties() const -> void {
    }
}