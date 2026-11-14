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

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/SettingsPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace imgui;
    using namespace scene;

    auto SettingsPanel::BeginProperties( core::cstr id ) -> bool {
        MKT_BEGIN_PROFILER();

        const core::f32 labelWidth{ eastl::clamp( ImGui::GetContentRegionAvail().x * 0.4f, 80.0f, 150.0f ) };

        if ( !ImGui::BeginTable( id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        ImGui::TableSetupColumn( MKT_LOC( "settings_label" ).c_str(), ImGuiTableColumnFlags_WidthFixed, labelWidth );
        ImGui::TableSetupColumn( MKT_LOC( "settings_value" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
        return true;
    }

    auto SettingsPanel::Property( core::cstr label ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( label );
        ImGui::TableSetColumnIndex( 1 );
        ImGui::SetNextItemWidth( -1.0f );
    }

    SettingsPanel::SettingsPanel( const SettingsPanelCreateInfo& data )
        : Panel{ "Settings", "Panel", data.mGroup, data.mIsToggleable }, mEditorState{ data.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_CONSTRUCTION, mPanelName );
    }

    auto SettingsPanel::DrawNavigation() -> void {
        MKT_BEGIN_PROFILER();
        auto& camera{ *mEditorState->mEditorCamera };

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "settings_navigation", "Navigation" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        if ( BeginProperties( "NavigationProperties" ) ) {
            Property( MKT_LOC( "settings_camera_mode" ).c_str() );

            if ( ImGui::BeginCombo( "##CameraMode", camera.GetNavigationMode() == scene::CameraNavigationMode::eOrbit ? MKT_LOC( "settings_orbit_look_at" ).c_str() : MKT_LOC( "settings_fly" ).c_str() ) ) {
                const auto modeOption = [&]( core::cstr label, scene::CameraNavigationMode mode ) {
                    const bool selected{ camera.GetNavigationMode() == mode };

                    if ( ImGui::Selectable( label, selected ) ) {
                        mEditorState->mEditorCamera->SetNavigationMode( mode );
                    }

                    if ( selected ) {
                        ImGui::SetItemDefaultFocus();
                    }
                };

                modeOption( MKT_LOC( "settings_fly" ).c_str(), scene::CameraNavigationMode::eFly );
                modeOption( MKT_LOC( "settings_orbit_look_at" ).c_str(), scene::CameraNavigationMode::eOrbit );
                ImGui::EndCombo();
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_fly_turns_at_the_camera_position_orbit_moves_around_a" ).c_str() );

            if ( camera.GetNavigationMode() == scene::CameraNavigationMode::eOrbit ) {
                auto target{ camera.GetOrbitTarget() };
                Property( MKT_LOC( "settings_orbit_pivot" ).c_str() );

                if ( ImGui::DragFloat3( "##OrbitPivot", glm::value_ptr( target ), 0.1f, -1000000.0f, 1000000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp ) ) {
                    camera.LookAt( camera.GetPosition(), target );
                }

                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_world_space_point_to_orbit_editing_it_aims_at_the" ).c_str() );
                auto distance{ camera.GetOrbitDistance() };
                Property( MKT_LOC( "settings_pivot_distance" ).c_str() );

                if ( ImGui::DragFloat( "##OrbitDistance", &distance, 0.1f, 0.01f, 1000000.0f, "%.2f u", ImGuiSliderFlags_AlwaysClamp ) ) {
                    camera.SetOrbitDistance( distance );
                }

                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_choose_the_pivot_along_the_current_view_ray_use_w" ).c_str() );
                Property( MKT_LOC( "settings_focus_selection" ).c_str() );
                ImGui::BeginDisabled( !mEditorState->mSelectedEntity );

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "settings_use_selected_entity", "Use selected entity" ).c_str() ) ) {
                    mEditorState->ValidateSelection();

                    if ( auto* selected{ mEditorState->mSelectedEntity } ) {
                        camera.LookAt( camera.GetPosition(), core::float3{ selected->GetComponent<scene::TransformComponent>().GetWorldTransform()[3] } );
                    }
                }

                ImGui::EndDisabled();
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_aim_at_the_selected_entity_s_world_origin_and_orbit" ).c_str() );
            }

            Property( MKT_LOC( "settings_move_speed" ).c_str() );
            auto movementSpeed{ camera.GetMovementSpeed() };

            if ( ImGui::DragFloat( "##MoveSpeed", &movementSpeed, 0.5f, 0.01f, 5000.0f, "%.2f u/s", ImGuiSliderFlags_AlwaysClamp ) ) {
                camera.SetMovementSpeed( movementSpeed );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_world_units_per_second_hold_right_mouse_in_the_scene" ).c_str() );
            Property( MKT_LOC( "settings_look_sensitivity" ).c_str() );
            auto rotationSpeed{ camera.GetRotationSpeed() };

            if ( ImGui::DragFloat( "##LookSpeed", &rotationSpeed, 0.1f, 0.1f, 500.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp ) ) {
                camera.SetRotationSpeed( rotationSpeed );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_higher_values_turn_the_camera_farther_for_the_same_mouse" ).c_str() );
            Property( MKT_LOC( "settings_smoothing" ).c_str() );
            auto smoothTime{ camera.GetSmoothTime() };

            if ( ImGui::DragFloat( "##SmoothTime", &smoothTime, 0.005f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp ) ) {
                camera.SetSmoothTime( smoothTime );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_approximate_response_time_for_movement_and_rotation_higher_is_softer" ).c_str() );
            Property( MKT_LOC( "settings_pitch_x" ).c_str() );
            auto pitchEnabled{ camera.WantsXAxisRotation() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "settings_allow_up_down", "Allow up / down" ).c_str(), pitchEnabled ) ) {
                camera.WantRotation( pitchEnabled, camera.WantsYAxisRotation() );
            }

            Property( MKT_LOC( "settings_yaw_y" ).c_str() );
            auto yawEnabled{ camera.WantsYAxisRotation() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "settings_allow_left_right", "Allow left / right" ).c_str(), yawEnabled ) ) {
                camera.WantRotation( camera.WantsXAxisRotation(), yawEnabled );
            }

            ImGui::EndTable();
        }

        if ( camera.GetNavigationMode() == scene::CameraNavigationMode::eOrbit ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "settings_hold_right_mouse_to_orbit_a_d_and_q_e" ).c_str() );
        }

        ImGui::Spacing();
    }

    auto SettingsPanel::DrawLens() -> void {
        MKT_BEGIN_PROFILER();
        auto& camera{ *mEditorState->mEditorCamera };

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "settings_lens", "Lens" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        if ( BeginProperties( "LensProperties" ) ) {
            Property( MKT_LOC( "settings_projection" ).c_str() );
            core::i32 projection{ camera.GetProjectionType() == scene::ProjectionType::eOrthographic ? 1 : 0 };

            if ( ImGui::Combo( "##Projection", &projection, MKT_LOC( "settings_perspective_orthographic" ).c_str() ) ) {
                camera.SetProjectionType( projection == 1 ? scene::ProjectionType::eOrthographic : scene::ProjectionType::ePerspective );
            }

            if ( camera.GetProjectionType() == scene::ProjectionType::ePerspective ) {
                Property( MKT_LOC( "settings_field_of_view" ).c_str() );
                auto fieldOfView{ camera.GetFOV() };

                if ( ImGui::DragFloat( "##FieldOfView", &fieldOfView, 0.2f, 1.0f, 170.0f, MKT_LOC( "settings_deg" ).c_str(), ImGuiSliderFlags_AlwaysClamp ) ) {
                    camera.SetFieldOfView( fieldOfView );
                }

                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_vertical_viewing_angle_wider_angles_show_more_of_the_scene" ).c_str() );
            }
            else {
                Property( MKT_LOC( "settings_view_height" ).c_str() );
                auto orthographicHeight{ camera.GetOrthographicHeight() };

                if ( ImGui::DragFloat( "##OrthographicHeight", &orthographicHeight, 0.1f, 0.01f, 1000000.0f, "%.2f u", ImGuiSliderFlags_AlwaysClamp ) ) {
                    camera.SetOrthographicHeight( orthographicHeight );
                }

                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_visible_height_in_world_units_smaller_values_zoom_in_width" ).c_str() );
            }

            Property( MKT_LOC( "settings_near_clip" ).c_str() );
            auto nearPlane{ camera.GetNearPlane() };

            if ( ImGui::DragFloat( "##NearClip", &nearPlane, 0.01f, 0.001f, eastl::min( 100000.0f, camera.GetFarPlane() - 0.01f ), "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
                camera.SetClipPlanes( nearPlane, camera.GetFarPlane() );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_closest_visible_distance_keeping_this_reasonably_large_improves_depth_precision" ).c_str() );
            Property( MKT_LOC( "settings_far_clip" ).c_str() );
            auto farPlane{ camera.GetFarPlane() };

            if ( ImGui::DragFloat( "##FarClip", &farPlane, 1.0f, camera.GetNearPlane() + 0.01f, 1000000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp ) ) {
                camera.SetClipPlanes( camera.GetNearPlane(), farPlane );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_farthest_visible_distance_must_be_greater_than_the_near_clip" ).c_str() );
            ImGui::EndTable();
        }

        ImGui::Spacing();
    }

    auto SettingsPanel::DrawView() -> void {
        MKT_BEGIN_PROFILER();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "settings_view", "View" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        auto& camera{ *mEditorState->mEditorCamera };
        auto position{ camera.GetPosition() };
        auto direction{ camera.GetTargetForward() };
        core::f32 yaw{ glm::degrees( glm::atan2( direction.x, -direction.z ) ) };
        core::f32 pitch{ glm::degrees( glm::asin( glm::clamp( direction.y, -1.0f, 1.0f ) ) ) };
        bool poseChanged{};
        bool anglesChanged{};

        if ( BeginProperties( "ViewProperties" ) ) {
            Property( MKT_LOC( "settings_position" ).c_str() );
            poseChanged = ImGui::DragFloat3( "##Position", glm::value_ptr( position ), 0.1f, -1000000.0f, 1000000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp );
            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_editor_camera_position_in_world_units_x_y_z" ).c_str() );
            Property( MKT_LOC( "settings_yaw" ).c_str() );
            anglesChanged |= ImGui::DragFloat( "##Yaw", &yaw, 0.2f, -180.0f, 180.0f, MKT_LOC( "settings_deg" ).c_str(), ImGuiSliderFlags_AlwaysClamp );
            Property( MKT_LOC( "settings_pitch" ).c_str() );
            anglesChanged |= ImGui::DragFloat( "##Pitch", &pitch, 0.2f, -89.0f, 89.0f, MKT_LOC( "settings_deg" ).c_str(), ImGuiSliderFlags_AlwaysClamp );
            ImGui::EndTable();
        }

        if ( anglesChanged ) {
            const core::f32 yawRadians{ glm::radians( yaw ) };
            const core::f32 pitchRadians{ glm::radians( pitch ) };
            const core::f32 cosPitch{ glm::cos( pitchRadians ) };
            direction = { glm::sin( yawRadians ) * cosPitch, glm::sin( pitchRadians ), -glm::cos( yawRadians ) * cosPitch };

            if ( camera.GetNavigationMode() == scene::CameraNavigationMode::eOrbit && !poseChanged ) {
                position = camera.GetOrbitTarget() - direction * camera.GetOrbitDistance();
            }

            poseChanged = true;
        }

        if ( poseChanged ) {
            camera.SetViewPose( position, direction );
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "settings_reset_view", "Reset view" ).c_str() ) ) {
            camera.ResetView();
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_return_to_the_starting_viewpoint_lens_and_navigation_settings_are" ).c_str() );
        ImGui::Spacing();
    }

    auto SettingsPanel::DrawDisplay() -> void {
        MKT_BEGIN_PROFILER();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "settings_display", "Display" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        auto* system{ renderer::RenderSystem::GetPtr() };

        if ( !system ) {
            return;
        }

        auto* context{ system->GetContext() };

        if ( !context ) {
            return;
        }

        bool vsync{ context->IsRefreshType( renderer::rhi::RefreshRate::eSync ) };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "settings_vertical_sync", "Vertical sync" ).c_str(), vsync ) ) {
            context->SetRefreshRate( vsync ? renderer::rhi::RefreshRate::eSync : renderer::rhi::RefreshRate::eUnlimited );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_synchronize_presentation_with_the_display_refresh_rate_applies_to_the" ).c_str() );
    }

    auto SettingsPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mPanelIsVisible ) {
            return;
        }

        const imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_WindowPadding, ImVec2{ 16.0f, 14.0f } };
        const imgui::ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 10.0f } };
        const imgui::ImGuiScopedStyleVar cellPadding{ ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, 5.0f } };

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            if ( !mEditorState || !mEditorState->mEditorCamera ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "settings_editor_camera_is_unavailable" ).c_str() );
            }
            else {
                ImGui::TextUnformatted( MKT_LOC( "settings_editor_camera" ).c_str() );
                {
                    const imgui::ImGuiScopedColor textColor{ ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) };
                    ImGui::TextWrapped( "%s", mEditorState->mHasProject ? MKT_LOC( "settings_camera_settings_will_be_saved_with_the_project" ).c_str() : MKT_LOC( "settings_camera_settings_for_this_sandbox_session" ).c_str() );
                }

                ImGui::Spacing();

                // Settings always address the editor camera, even when Play
                // temporarily switches mActiveCamera to a scene-owned camera.
                ImGui::BeginDisabled( mEditorState->mRuntimeActive );

                DrawNavigation();
                DrawLens();
                DrawView();
                DrawDisplay();

                ImGui::EndDisabled();

                // Framed section headers extend into the padding on both sides.
                const core::f32 buttonIndent{ ImGui::GetStyle().FramePadding.x };
                ImGui::Unindent( buttonIndent );
                const ImVec2 buttonSize{ ImGui::GetContentRegionAvail().x + buttonIndent, ImGui::GetFrameHeight() + 8.0f };

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "settings_reset_camera_settings", "Reset camera settings" ).c_str(), buttonSize ) ) {
                    mEditorState->mEditorCamera->ResetSettings();
                }

                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "settings_reset_navigation_and_lens_values_while_keeping_your_current_viewpoint" ).c_str() );
                ImGui::Indent( buttonIndent );
            }
        }

        ImGui::End();
    }

}// namespace mikoto::editor
