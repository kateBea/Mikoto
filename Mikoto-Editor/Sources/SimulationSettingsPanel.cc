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

#include <Core/LocalizationService.hh>

#include <Physics/PhysicsWorld.hh>

#include <Scene/Scene.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Panels/SimulationSettingsPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    using namespace mikoto::physics;

    SimulationSettingsPanel::SimulationSettingsPanel( const SimulationSettingsPanelCreateInfo& info )
        : Panel{ "Simulation Settings", "Simulation Settings", info.mGroup, info.mIsToggleable },
          mState{ info.mState } {
        mPanelIsVisible = false;
    }

    auto SimulationSettingsPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 560, 400 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        PhysicsWorld* physicsWorld{ mState->mActiveScene ? mState->mActiveScene->GetPhysicsWorld() : nullptr };

        if ( physicsWorld ) {
            auto gravity{ physicsWorld->GetGravity() };

            if ( ImGui::DragFloat3( imgui::widget::LocalizedLabel( "simulation_settings_gravity_m_s", "Gravity (m/s²)" ).c_str(), &gravity.x, 0.05f, -1000, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
                physicsWorld->SetGravity( gravity );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "simulation_settings_earth_gravity", "Earth gravity" ).c_str() ) ) {
                physicsWorld->SetGravity( { 0, -9.80665f, 0 } );
            }

            ImGui::Text( MKT_LOC( "simulation_settings_bodies" ).c_str(), static_cast<unsigned long long>( physicsWorld->GetBodyCount() ) );
            ImGui::SeparatorText( MKT_LOC( "simulation_settings_collision_layers" ).c_str() );

            ImGui::TextUnformatted( MKT_LOC( "simulation_settings_collision_layer_rules" ).c_str() );

            ImGui::TextWrapped( "%s", MKT_LOC( "simulation_settings_the_current_broad_phase_uses_two_engine_defined_layers_body" ).c_str() );
            ImGui::TextWrapped( "%s", MKT_LOC( "simulation_settings_gravity_applies_to_the_active_scene_during_play_changes_affect" ).c_str() );
        }
        else {
            ImGui::TextDisabled( "%s", MKT_LOC( "simulation_settings_open_a_scene_to_configure_simulation" ).c_str() );
        }

        ImGui::End();
    }
}// namespace mikoto::editor
