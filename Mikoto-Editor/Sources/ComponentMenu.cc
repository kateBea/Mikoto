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

#include <imgui.h>

#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Scene/Entity.hh>
#include <Scene/Utility.hh>
#include <Scene/Component.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Panels/ComponentMenu.hh>

namespace mikoto::editor {
    template<typename Component>
    static auto ComponentMenuItem( scene::Entity& entity, const ImGuiTextFilter& filter, core::cstr label, core::cstr translation = nullptr ) -> void {
        const eastl::string text{ translation ? MKT_LOC( translation ) : eastl::string{ label } };

        if ( !filter.PassFilter( text.c_str() ) ) {
            return;
        }

        bool available{ !entity.HasComponent<Component>() };

        if constexpr ( eastl::is_base_of_v<scene::JointComponent, Component> ) {
            scene::ForEachJointComponent( [&]<typename T>( const char* ) {
                available &= !entity.HasComponent<T>();
            } );
        }

        ImGui::PushID( label );

        if ( ImGui::MenuItem( text.c_str(), nullptr, false, available ) ) {
            auto& component{ entity.AddComponent<Component>() };

            if constexpr ( eastl::is_same_v<Component, scene::TextComponent> ) {
                component.SetSize( 12 );
                component.SetContents( "Example" );
                component.SetSpacing( 1 );
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::PopID();
    }

    auto DrawAddComponentMenu( scene::Entity* entity ) -> void {
        if ( !entity ) {
            return;
        }

        static ImGuiTextFilter filter{};
        imgui::widget::SearchFilter( "##ComponentSearch", filter,
            MKT_LOC( "hierarchy_search_filter" ), 220.0f );

        if ( filter.IsActive() || ImGui::BeginMenu( "Core" ) ) {
            ComponentMenuItem<scene::TransformComponent>( *entity, filter, "Transform" );

            if ( !filter.IsActive() ) {
                ImGui::EndMenu();
            }
        }

        if ( filter.IsActive() || ImGui::BeginMenu( "Rendering" ) ) {
            ComponentMenuItem<scene::MeshComponent>( *entity, filter, "Mesh", "component_menu_mesh" );
            ComponentMenuItem<scene::VideoComponent>( *entity, filter, "Video" );
            ComponentMenuItem<scene::MaterialComponent>( *entity, filter, "Physical Material", "component_menu_physical_material" );
            ComponentMenuItem<scene::SkyboxMaterialComponent>( *entity, filter, "Skybox Material", "component_menu_skybox_material" );
            ComponentMenuItem<scene::PostProcessMaterialComponent>( *entity, filter, "Post-Process Material", "component_menu_post_process_material" );
            ComponentMenuItem<scene::CameraComponent>( *entity, filter, "Camera", "component_menu_camera" );
            ComponentMenuItem<scene::LightComponent>( *entity, filter, "Lighting", "component_menu_lighting" );
            ComponentMenuItem<scene::AnimatorComponent>( *entity, filter, "Animator", "component_menu_animator" );
            ComponentMenuItem<scene::SkinnedMeshRenderer>( *entity, filter, "Skinned Mesh Renderer", "component_menu_skinned_mesh_renderer" );
            ComponentMenuItem<scene::FogComponent>( *entity, filter, "Fog", "component_menu_fog" );
            ComponentMenuItem<scene::CloudComponent>( *entity, filter, "Cloud", "volume_cloud" );
            ComponentMenuItem<scene::WaterRenderComponent>( *entity, filter, "Water", "component_menu_water" );
            ComponentMenuItem<scene::WaterSimulationComponent>( *entity, filter, "Water simulation", "water_simulation" );
            ComponentMenuItem<scene::TextureRenderComponent>( *entity, filter, "Texture Render", "component_menu_texture_render" );
            ComponentMenuItem<scene::ParticleSimulatorComponent>( *entity, filter, "Particle effect", "particle_effect_asset" );

            if ( !filter.IsActive() ) {
                ImGui::EndMenu();
            }
        }

        if ( filter.IsActive() || ImGui::BeginMenu( "Physics" ) ) {
            ComponentMenuItem<scene::RigidBodyComponent>( *entity, filter, "Rigid body", "component_menu_rigid_body" );
            ComponentMenuItem<scene::BoxColliderComponent>( *entity, filter, "Box collider" );
            ComponentMenuItem<scene::SphereColliderComponent>( *entity, filter, "Sphere collider" );
            ComponentMenuItem<scene::CapsuleColliderComponent>( *entity, filter, "Capsule collider" );
            ComponentMenuItem<scene::MeshColliderComponent>( *entity, filter, "Mesh collider" );
            ComponentMenuItem<scene::VehicleComponent>( *entity, filter, "Vehicle" );
            ComponentMenuItem<scene::VehicleWheelComponent>( *entity, filter, "Vehicle wheel" );
            scene::ForEachExtendedPhysicsComponent( [&]<typename T>( const char* name ) {
                eastl::string label{ name };
                label.erase( label.size() - eastl::string_view{ "Component" }.size() );
                ComponentMenuItem<T>( *entity, filter, label.c_str() );
            } );

            if ( !filter.IsActive() ) {
                ImGui::EndMenu();
            }
        }

        if ( filter.IsActive() || ImGui::BeginMenu( "Audio" ) ) {
            ComponentMenuItem<scene::AudioSourceComponent>( *entity, filter, "Audio", "component_menu_audio" );
            ComponentMenuItem<scene::AudioListenerComponent>( *entity, filter, "Audio listener", "component_menu_audio_listener" );

            if ( !filter.IsActive() ) {
                ImGui::EndMenu();
            }
        }

        if ( filter.IsActive() || ImGui::BeginMenu( "Scripting" ) ) {
            ComponentMenuItem<scene::ScriptComponent>( *entity, filter, "Script", "component_menu_script" );

            if ( !filter.IsActive() ) {
                ImGui::EndMenu();
            }
        }

        if ( filter.IsActive() || ImGui::BeginMenu( "UI" ) ) {
            ComponentMenuItem<scene::TextComponent>( *entity, filter, "Text", "component_menu_text" );
            ComponentMenuItem<scene::CanvasComponent>( *entity, filter, "Canvas", "component_menu_canvas" );

            if ( !filter.IsActive() ) {
                ImGui::EndMenu();
            }
        }
    }
}// namespace mikoto::editor
