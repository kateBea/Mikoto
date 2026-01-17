//
// Created by zanet on 1/9/2026.
//

#include <Panels/ScenePropertiesPanel.hh>

// Third-Party Libraries
#include "fmt/format.h"
#include "imgui.h"

// Project Headers
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>

#include "Scene/Component.hh"

namespace Mikoto {

    ScenePropertiesPanel::ScenePropertiesPanel( const ScenePropertiesPanelCreateInfo &info )
        : Panel{ "Scene Properties" }, m_EditorState{ info.State }
    {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_DATA_OBJECT, m_PanelName );
    }

    auto ScenePropertiesPanel::OnUpdate( float timeStep ) -> void {
        // Display info about clustered forward
        // this is mainly a debug pass
        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::Separator();
        ImGui::Text( "Skybox");

        float gamma    { m_EditorState->ActiveEditorScene->GetGamma() };
        float exposure { m_EditorState->ActiveEditorScene->GetExposure() };

        ImGui::SliderFloat("Gamma", &gamma, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Exposure", &exposure, 0.0f, 10.0f, "%.2f");

        // Optional: Show preview or values
        ImGui::Text("Gamma: %.2f  Exposure: %.2f", gamma, exposure);

        m_EditorState->ActiveEditorScene->SetGamma(gamma);
        m_EditorState->ActiveEditorScene->SetExposure(exposure);

        ImGui::End();
    }
}