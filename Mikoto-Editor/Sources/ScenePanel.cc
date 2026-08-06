//    Copyright 2025 ケイト
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

#include <memory>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

// Important to include after imgui
#include <ImGuizmo.h>
#include <ImOGuizmo.hh>

#include <ImGui/IconsMaterialDesign.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Math/Math.hh>

#include <Memory/Allocator.hh>

#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Layers/EditorLayer.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Panels/ScenePanel.hh>

namespace mikoto::editor {

    using namespace mikoto::gui;
    using namespace mikoto::core;
    using namespace mikoto::scene;

    MKT_NODISCARD static auto InferManipulationMode( gui::GuizmoType manipulation ) -> ImGuizmo::OPERATION {
        switch (manipulation) {
            case gui::GuizmoType::eTranslation:
                return ImGuizmo::OPERATION::TRANSLATE;
            case gui::GuizmoType::eRotation:
                return ImGuizmo::OPERATION::ROTATE;
            case gui::GuizmoType::eScale:
                return ImGuizmo::OPERATION::SCALE;

            default: ;
        }

        return ImGuizmo::OPERATION::TRANSLATE;
    }

    ScenePanel::ScenePanel( const ScenePanelCreateInfo &createInfo )
        : Panel{ "Scene" },  mEditorState{ createInfo.mState } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_IMAGE, mPanelName );
    }

    auto ScenePanel::OnUpdate( float ts ) -> void {
        if (!mPanelIsVisible) {
            return;
        }

        constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };

        // Expand scene view to window bounds (no padding)
        ImGuiScopedStyleVar winPad ( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), windowFlags );

        mPanelIsFocused = ImGui::IsWindowFocused();
        mPanelIsHovered = ImGui::IsWindowHovered();

        ImVec2 viewportMin{ ImGui::GetItemRectMin()};
        ImVec2 viewportSize{ ImGui::GetContentRegionAvail() };

        mViewport = ViewportInfo{
            .mX = viewportMin.x,
            .mY = viewportMin.y,
            .mWidth = viewportSize.x,
            .mHeight = viewportSize.y
        };

        UpdateViewport();

        UpdateManipulation();
        DrawManipulationGuizmos();
        DrawOrientationAxis();

        ImGui::End();
    }

    auto ScenePanel::SetManipulation( GuizmoType mode ) -> void {
        mManipulationType = mode;
    }

    auto ScenePanel::SetTexture( TextureHandle texture ) -> void {
        if (texture.IsEmpty()) {
            return;
        }

        mColorImageID = ImGuiService::Get()->GetTextureID( texture );
    }

    auto ScenePanel::GetWidth() const -> float {
        return mViewportWidth;
    }

    auto ScenePanel::GetHeight() const -> float {
        return mViewportHeight;
    }

    auto ScenePanel::DrawOrientationAxis() -> void {
        ImVec2 winPos{ ImGui::GetWindowPos() };

        const float widgetSize{ 100.0f };
        const float gizmoX{ winPos.x };
        const float gizmoY{ winPos.y + mViewportHeight - widgetSize };

        ImOGuizmo::SetRect(gizmoX, gizmoY, widgetSize);

        glm::mat4 view{ mEditorState->mActiveCamera->GetViewMatrix() };
        glm::mat4 proj{ glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f) };

        ImOGuizmo::DrawGizmo(glm::value_ptr(view), glm::value_ptr(proj));
    }

    auto ScenePanel::DrawUtilitiesOverlay() -> void {
    }

    auto ScenePanel::IsDisplayTextureValid() const -> bool {
        return mColorImageID != 0;
    }

    auto ScenePanel::UpdateViewport() -> void {
        const ImVec2 dim{ ImGui::GetContentRegionAvail() };

        if (mViewportWidth != dim.x || mViewportHeight != dim.y) {
            mViewportWidth = dim.x;
            mViewportHeight = dim.y;
        }

        // No flipping, the final image is already in the correct viewport coordinates
        // In the case of vulkan this is also taken into account when setting up the viewport
        if (IsDisplayTextureValid()) {
            ImGui::Image( mColorImageID, ImVec2{ dim.x, dim.y } );
        }
    }

    auto ScenePanel::UpdateManipulation() -> void {
        if (!mEditorState->mSelectedEntity) {
            return;
        }

        Entity *currentSelection{ mEditorState->mSelectedEntity };
        if (!currentSelection->IsValid() || !currentSelection->GetComponent<TagComponent>().IsActive()) {
            return;
        }

        ImGuizmo::SetOrthographic( mEditorState->mActiveCamera->IsOrthographic() );
        ImGuizmo::SetDrawlist();

        const ImVec2 windowPosition{ ImGui::GetWindowPos() };
        const ImVec2 windowDimensions{ ImGui::GetWindowSize() };
        ImGuizmo::SetRect( windowPosition.x, windowPosition.y, windowDimensions.x, windowDimensions.y );
    }

    auto ScenePanel::DrawManipulationGuizmos() -> void {
        Entity *currentSelection{ mEditorState->mSelectedEntity };
        if (currentSelection == nullptr || !currentSelection->IsValid() || !currentSelection->GetComponent<TagComponent>().IsActive()) {
            return;
        }

        TransformComponent &transformComponent{ currentSelection->GetComponent<TransformComponent>() };

        const float4x4 &cameraView{ mEditorState->mActiveCamera->GetViewMatrix() };
        const float4x4 &cameraProjection{ mEditorState->mActiveCamera->GetProjection() };

        float4x4 objectTransform{ transformComponent.GetTransform() };

        ImGuizmo::OPERATION operation{ InferManipulationMode( mManipulationType ) };
        ImGuizmo::Manipulate( glm::value_ptr( cameraView ), glm::value_ptr( cameraProjection ), operation, ImGuizmo::MODE::WORLD, glm::value_ptr( objectTransform ) );

        if (ImGuizmo::IsUsing()) {
            transformComponent.SetTransform( objectTransform );
        }
    }

    auto ScenePanel::DrawSceneToolbar() -> void {
        // Static so user dragging persists
        static bool firstFrame{ true };
        static ImVec2 toolbarPos{};

        const ImVec2 windowPos{ ImGui::GetWindowPos() };
        const ImVec2 windowSize{ ImGui::GetWindowSize() };

        // Initial center positioning
        if (firstFrame) {
            toolbarPos = ImVec2{
                windowPos.x + windowSize.x * 0.5f - 70.0f,
                windowPos.y + 20.0f
            };
            firstFrame = false;
        }

        ImGui::SetNextWindowPos( toolbarPos, ImGuiCond_Always );
        ImGui::SetNextWindowBgAlpha( 0.20f );// transparent inner

        // Styles
        ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.20f } );// inner transparent
        ImGui::PushStyleColor( ImGuiCol_Border, ImVec4{ 0.0f, 0.0f, 0.0f, 1.00f } );  // opaque border

        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 6.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1.3f );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 4.0f );

        ImGuiWindowFlags flags{
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
        };

        if (ImGui::Begin( "SceneToolsOverlay", nullptr, flags )) {
            // Drag anywhere inside window
            if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging( ImGuiMouseButton_Left )) {
                const ImVec2 delta{ ImGui::GetIO().MouseDelta };
                toolbarPos.x += delta.x;
                toolbarPos.y += delta.y;
            }

            auto makeTool = [&]( const char *icon, GuizmoType type ) {
                const bool active{
                    mManipulationType == as<gui::GuizmoType>( type )
                };

                const ImVec2 btnSize{ 28.0f, 28.0f };
                const ImVec2 iconPadding{ 2.0f, 2.0f };

                if (active) {
                    ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.75f, 0.75f, 0.75f, 0.85f } );
                }

                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, iconPadding );

                if (ImGui::Button( icon, btnSize )) {
                    mManipulationType = as<gui::GuizmoType>( type );
                }

                ImGui::PopStyleVar();

                if (active) ImGui::PopStyleColor();

                ImGui::SameLine();
            };

            // Extra spacing on first button
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 6.0f, 0.0f } );
            makeTool( ICON_MD_OPEN_WITH, gui::GuizmoType::eTranslation );
            ImGui::PopStyleVar();

            makeTool( ICON_MD_ROTATE_RIGHT, gui::GuizmoType::eRotation );
            makeTool( ICON_MD_OPEN_IN_FULL, gui::GuizmoType::eScale );

            ImGui::NewLine();
        }

        ImGui::End();

        ImGui::PopStyleVar( 3 );
        ImGui::PopStyleColor( 2 );
    }

}// namespace Mikoto