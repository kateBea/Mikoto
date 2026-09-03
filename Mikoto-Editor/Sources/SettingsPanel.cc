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

#include <EASTL/array.h>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>

#include <Layers/EditorLayer.hh>
#include <Panels/SettingsPanel.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    auto SettingsPanel::DrawCameraConfig() -> void {
        static const eastl::array<eastl::string, 2> cameraProjectionTypesStr{
            "Orthographic", "Perspective"
        };

        SceneCamera &sceneCamera{ *mEditorState->mActiveCamera };
        const ProjectionType cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

        const eastl::string &currentProjectionTypeStr{ cameraProjectionTypesStr[static_cast<u32>( cameraCurrentProjectionType )] };

        ImGui::Spacing();
        if ( ImGui::BeginCombo( "##SettingsPanel::OnUpdate::EditorCam:Projection", currentProjectionTypeStr.c_str() ) ) {
            u32 projectionIndex{};

            for ( const eastl::string &projectionType: cameraProjectionTypesStr ) {
                const bool isSelected{ projectionType == cameraProjectionTypesStr[as<u32>( cameraCurrentProjectionType )] };

                if ( ImGui::Selectable( fmt::format( " {}", projectionType ).c_str(), isSelected ) ) {
                    sceneCamera.SetProjectionType( as<ProjectionType>( projectionIndex ) );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }

                ++projectionIndex;
            }

            ImGui::EndCombo();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        if ( sceneCamera.GetProjectionType() == ProjectionType::PERSPECTIVE ) {
            ImGui::Spacing();
            (void)Slider( "##SettingsPanel::OnUpdate::CameraSpeed", mData.mEditorCameraMovementSpeed, { 2, 5000 } );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Adjust camera movement speed" );

            ImGui::Spacing();
            (void)Slider( "##SettingsPanel::OnUpdate::RotationSpeed", mData.mEditorCameraRotationSpeed, { 3, 500 } );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Adjust camera rotation speed." );

            ImGui::Spacing();
            if ( imgui::Slider( "##SettingsPanel::OnUpdate::Damping", mData.mDampingFactor, { 5, 30 } ) ) {
                mEditorState->mActiveCamera->SetDampingFactor( mData.mDampingFactor );
            }
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Adjust camera smooth damping factor." );

            ImGui::Spacing();
            (void)Slider( "##SettingsPanel::OnUpdate::NearClip", mData.mNearPlane, { -10.0f, 100.0f } );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Adjust camera near plane." );

            ImGui::Spacing();
            (void)Slider( "##SettingsPanel::OnUpdate::FarClip", mData.mFarPlane, { 5, 200000 } );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Adjust camera far clip." );

            ImGui::Spacing();
            (void)Slider( "##SettingsPanel::OnUpdate::FieldOfView", mData.mFieldOfView, { 10, 100 } );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Adjust camera field of view." );

            ImGui::Spacing();
            (void)CheckBox( "Lock Rotation ( X )", mData.mWantXAxisRotation );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Lock rotation in the X axis. Cannot look from top to bottom and viceversa." );

            ImGui::Spacing();
            (void)CheckBox( "Lock Rotation ( Y )", mData.mWantYAxisRotation );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Lock rotation in the X axis. Cannot look from left to right and viceversa." );

            ImGui::Spacing();
            (void)CheckBox( "Lock Camera to target", mData.mLockCameraToTarget );
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Lock camera to current selected entity" );

            // Limit FPS
            ImGui::Spacing();

            bool isVsyncEnabled{ RenderSystem::Get()->GetContext()->IsRefreshType( RefreshRate::eSync ) };

            if ( imgui::CheckBox( "Limit FPS", isVsyncEnabled ) ) {
                if (isVsyncEnabled) {
                    RenderSystem::Get()->GetContext()->SetRefreshRate( RefreshRate::eSync );
                } else {
                    RenderSystem::Get()->GetContext()->SetRefreshRate( RefreshRate::eUnlimited );
                }
            }
            ImGui::SameLine();
            widget::MakeHelpPopUp( "Enable Vertical Sync" );
        }

        if ( sceneCamera.GetProjectionType() == ProjectionType::ORTHOGRAPHIC ) {}
    }

    auto SettingsPanel::DrawCameraProperties() -> void {
        SceneCamera *camera{ mEditorState->mActiveCamera };
        if ( !camera )
            return;

        if ( ImGui::CollapsingHeader( "Camera", ImGuiTreeNodeFlags_DefaultOpen ) ) {
            float fov{ camera->GetFOV() };
            if ( ImGui::DragFloat( "FOV", &fov, 0.1f, 1.0f, 120.0f ) )
                camera->SetFieldOfView( fov );

            float nearPlane{ camera->GetNearPlane() };
            if ( ImGui::DragFloat( "Near Plane", &nearPlane, 0.01f, 0.001f, 10.0f ) )
                camera->SetNearPlane( nearPlane );

            float farPlane{ camera->GetFarPlane() };
            if ( ImGui::DragFloat( "Far Plane", &farPlane, 1.0f, 10.0f, 10000.0f ) )
                camera->SetFarPlane( farPlane );

            ImGui::Separator();

            float3 target{ camera->GetPosition() };
            float targetArr[3]{ target.x, target.y, target.z };

            if ( ImGui::DragFloat3( "Position", targetArr, 0.1f ) )
                camera->SetCameraTarget( { targetArr[0], targetArr[1], targetArr[2] } );
        }
    }

    SettingsPanel::SettingsPanel( const SettingsPanelCreateInfo &data )
        : Panel{ "Settings" }, mEditorState( data.mState ) {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_CONSTRUCTION, mPanelName );
    }

    auto SettingsPanel::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mPanelIsVisible) {
            return;
        }

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        imgui::DrawNode( "Camera", [this] () -> void {
            DrawCameraConfig();
        } );

        imgui::DrawNode( "Camera properties", [this] () -> void {
            DrawCameraProperties();
        } );

        ImGui::End();
    }

    auto SettingsPanel::GetData() -> SettingsPanelData & {
        return mData;
    }

    auto SettingsPanel::GetData() const -> const SettingsPanelData & {
        return mData;
    }
}// namespace Mikoto