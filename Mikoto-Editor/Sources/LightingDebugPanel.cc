//
// Created by zanet on 1/6/2026.
//


#include <array>
#include <string_view>
#include <typeinfo>

// Third-Party Libraries
#include "fmt/format.h"
#include "imgui.h"

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Passes/ClusteredShading.hh>

#include <Panels/LightingDebugPanel.hh>

#include "Scene/Component.hh"

namespace Mikoto {

    static auto DrawVec3ReadOnly( const char *label, const glm::vec3 &v ) -> void {
        ImGui::Text( "%s:", label );
        ImGui::SameLine();
        ImGui::Text( "(%.2f, %.2f, %.2f)", v.x, v.y, v.z );
    }

    static auto DrawFloatReadOnly( const char *label, float value, const char *suffix = "" ) -> void {
        ImGui::Text( "%s:", label );
        ImGui::SameLine();
        ImGui::Text( "%.2f%s", value, suffix );
    }

    static auto DrawColorReadOnly( const glm::vec3 &color ) -> void {
        ImGui::ColorButton(
                "##LightColor",
                ImVec4{ color.r, color.g, color.b, 1.0f },
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                ImVec2{ 32.0f, 16.0f }
                );

        ImGui::SameLine();
        ImGui::Text( "(%.2f, %.2f, %.2f)", color.r, color.g, color.b );
    }

    static auto DrawLightBaseInfo( const LightObject &light ) -> void {
        ImGui::SeparatorText( "Base Properties" );

        ImGui::TextUnformatted( "Color" );
        ImGui::SameLine( 120.0f );
        DrawColorReadOnly( light.GetColor() );

        ImGui::TextUnformatted( "Intensity" );
        ImGui::SameLine( 120.0f );
        ImGui::Text( "%.2f", light.GetIntensity() );
    }

    static auto DrawPointLightInfo( const PointLight &light ) -> void {
        ImGui::SeparatorText( "Point Light" );

        DrawLightBaseInfo( light );

        ImGui::SeparatorText( "Attenuation" );

        ImGui::TextUnformatted( "Radius" );
        ImGui::SameLine( 120.0f );
        ImGui::Text( "%.2f", light.GetRadius() );
    }

    static auto DrawDirectionalLightInfo( const DirectionalLight &light ) -> void {
        ImGui::SeparatorText( "Directional Light" );

        DrawLightBaseInfo( light );

        ImGui::SeparatorText( "Direction" );
        DrawVec3ReadOnly( "Direction", light.GetDirection() );
    }

    static auto DrawSpotLightInfo( const SpotLight &light ) -> void {
        ImGui::SeparatorText( "Spot Light" );

        DrawLightBaseInfo( light );

        ImGui::SeparatorText( "Cone" );

        DrawVec3ReadOnly( "Direction", light.GetDirection() );
        DrawFloatReadOnly( "Inner Cutoff", light.GetCutOff(), " rad" );
        DrawFloatReadOnly( "Outer Cutoff", light.GetOuterCutOff(), " rad" );

        ImGui::SeparatorText( "Range" );
        DrawFloatReadOnly( "Radius", light.GetRadius() );
    }

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

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::Separator();
        ImGui::Text( "FPS: %.1f", 1.0f / timeStep );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text( "TimeStep: %.1f ms", timeStep * 1000.0f );
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text( "Scene Name: %s", m_EditorState->ActiveEditorScene->GetName().c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text( "Total lights: %d", m_EditorState->ActiveEditorScene->GetLightCount() );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text( "Total active lights: %d", m_EditorState->ActiveEditorScene->GetActiveLightCount() );

        DisplaySelectedLightProperties();

        FrameBlackboard* frameBlackboard{ m_EditorState->EditorSceneRenderer->GetGraph().GetBlackboard() };
        BufferHandle storage{ frameBlackboard->GetBuffer( "SimpleComputePass_Result" ) };

        // Display list of buffers involved and make it to display info about selected buffer

        // Example buffer display
        static std::vector<UInt32> data(40);
        storage->CopyToBlock( data.data(), data.size() * sizeof( UInt32 ) );

        const void* bufferMemory{ data.data() };
        const Size bufferSize{ data.size() * sizeof( UInt32 ) };
        const std::uintptr_t baseAddress{ reinterpret_cast<const std::uintptr_t>( data.data() ) };

        ImGui::SeparatorText( "Buffer Memory" );
        ImGuiUtils::DrawMemoryVisualizer(bufferMemory, bufferSize, baseAddress, 1);

        ImGui::End();
    }

    auto LightingDebugPanel::DisplaySelectedLightProperties() const -> void {
        if (m_EditorState->SelectedEntity == nullptr) { return; }

        if (!m_EditorState->SelectedEntity->HasComponent<LightComponent>()) { return; }

        LightComponent &lightComp{ m_EditorState->SelectedEntity->GetComponent<LightComponent>() };

        ImGui::Spacing();
        ImGui::SeparatorText( "Selected Light Debug" );

        switch (lightComp.GetActiveType()) {
            case LightType::POINT_LIGHT_TYPE:
                DrawPointLightInfo( lightComp.Get<PointLight>() );
                break;

            case LightType::DIRECTIONAL_LIGHT_TYPE:
                DrawDirectionalLightInfo( lightComp.Get<DirectionalLight>() );
                break;

            case LightType::SPOT_LIGHT_TYPE:
                DrawSpotLightInfo( lightComp.Get<SpotLight>() );
                break;
        }

        // Lighting passes
        AABBGenComp* aabbGenComPass{ m_EditorState->EditorSceneRenderer->GetPass<AABBGenComp>() };
        LightCullingComp* lightCullingComp{ m_EditorState->EditorSceneRenderer->GetPass<LightCullingComp>() };
        FinalCompositionPass* finalCompositionPass{ m_EditorState->EditorSceneRenderer->GetPass<FinalCompositionPass>() };

        FrameBlackboard* frameBlackboard{ m_EditorState->EditorSceneRenderer->GetGraph().GetBlackboard() };
        BufferHandle storage{ frameBlackboard->GetBuffer( "SimpleComputePass_Result" ) };

        // Display list of buffers involved and make it to display info about selected buffer

    }
}