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

#include "ImGui/ImGuiService.hh"
#include "Scene/Component.hh"
#include <Physics/PhysicsWorld.hh>

namespace Mikoto {

    ScenePropertiesPanel::ScenePropertiesPanel( const ScenePropertiesPanelCreateInfo &info )
        : Panel{ "Scene Properties" }, m_EditorState{ info.State }
    {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_DATA_OBJECT, m_PanelName );
    }

    auto ScenePropertiesPanel::OnUpdate( float timeStep ) -> void {
        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGuiUtils::DrawNode( "Background", [this]() -> void {
            ImGuiUtils::UnindentScoped und{};

            ImGui::Separator();
            ImGui::Text( "Skybox" );

            float gamma{ m_EditorState->ActiveEditorScene->GetGamma() };
            float exposure{ m_EditorState->ActiveEditorScene->GetExposure() };

            ImGui::SliderFloat( "Gamma", &gamma, 0.1f, 5.0f, "%.2f" );
            ImGui::SliderFloat( "Exposure", &exposure, 0.0f, 10.0f, "%.2f" );

            m_EditorState->ActiveEditorScene->SetGamma( gamma );
            m_EditorState->ActiveEditorScene->SetExposure( exposure );
        } );

        ImGuiUtils::DrawNode( "Scene Stats", [this]() -> void {
            ImGuiUtils::UnindentScoped und{};

            // TODO
        } );

        ImGuiUtils::DrawNode( "Metadata", [this]() {
            ImGuiUtils::UnindentScoped und{};

            // TODO
        } );

        ImGuiUtils::DrawNode( "Physics world", [this]() -> void {
            ImGuiUtils::UnindentScoped und{};

            auto scene{ m_EditorState->ActiveEditorScene };
            auto *physics{ scene->GetPhysicsWorld() };

            static std::array<std::string, 4> values{
                "Earth", "Moon", "Mars", "Jupiter"
            };

            GravityBody current{ physics->GetGravityBody() };
            current = ImGuiUtils::Combo( values, current );

            physics->SetGravityBody( current );
        } );

        ImGui::End();
    }
}