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
#include <Core/RuntimeConsole.hh>

#include <Math/Math.hh>

#include <Memory/Allocator.hh>

#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Threading/TaskManager.hh>

#include <Layers/EditorLayer.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Panels/ScenePanel.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto InferManipulationMode( imgui::GizmoType manipulation ) -> ImGuizmo::OPERATION {
        switch (manipulation) {
            case imgui::GizmoType::eTranslation:
                return ImGuizmo::OPERATION::TRANSLATE;
            case imgui::GizmoType::eRotation:
                return ImGuizmo::OPERATION::ROTATE;
            case imgui::GizmoType::eScale:
                return ImGuizmo::OPERATION::SCALE;

            default: ;
        }

        return ImGuizmo::OPERATION::TRANSLATE;
    }

    ScenePanel::ScenePanel( const ScenePanelCreateInfo &createInfo )
        : Panel{ "Scene" },  mEditorState{ createInfo.mState } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_IMAGE, mPanelName );
    }

    auto ScenePanel::OnRender( float ts ) -> void {
        if (!mPanelIsVisible) {
            return;
        }

        constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };

        // Expand scene view to window bounds (no padding)
        ImGuiScopedStyleVar winPad ( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), windowFlags );
        // Using the window itself does not work for drag and drop, it does not
        // seem to be flagged as hovered when drag and drop is up

        // Save the original cursor position where ImGui was about to draw your UI layout.
        mStartCursorPos = ImGui::GetCursorPos();

        // Stretch an invisible element across the entire remaining content region of this window.
        // This acts as a "safety net" to catch mouse hover events over transparent areas.
        ImGui::Dummy(ImGui::GetContentRegionAvail());

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload(
                "CONTENT_BROWSER_TEXT_MODEL", ImGuiDragDropFlags_AcceptNoDrawDefaultRect) }) {
                Path path{ *as<Path*>( payload->Data ) };

                if (!path.IsEmpty()) {
                    threading::TaskService::Get()->Submit( [this, path]() -> void {
                        ModelLoadDescription description{
                            .mFile = FileService::Get()->LoadFile( path ),
                            .mExtractTextures = true };
                        const ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( description ) };

                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ description.mFile->GetName() },
                            .mModel = model };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    } );
                }

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_MODEL" );
            }
            ImGui::EndDragDropTarget();
        }

        // Restore the cursor position back to the top-left starting point.
        // This ensures any overlay buttons, gizmos, or transformation tools drawn below
        // will render directly on top of the drop zone without being pushed down.
        ImGui::SetCursorPos(mStartCursorPos);

        mPanelIsFocused = ImGui::IsWindowFocused();
        mPanelIsHovered = ImGui::IsWindowHovered();

        ImVec2 viewportMin{ ImGui::GetItemRectMin()};
        ImVec2 viewportSize{ ImGui::GetContentRegionAvail() };

        mViewport = ViewportInfo{
            .mX = viewportMin.x,
            .mY = viewportMin.y,
            .mWidth = viewportSize.x,
            .mHeight = viewportSize.y };

        DrawFinalImage();

        ImGui::SetCursorPos(mStartCursorPos);

        DrawSceneButtons();
        DrawTransformGizmos();
        DrawUtilitiesOverlay();

        UpdateManipulation();
        DrawManipulationGizmos();
        DrawOrientationAxis();

        ImGui::End();
    }

    auto ScenePanel::SetGizmoType( GizmoType type ) -> void {
        mGizmoType = type;
    }

    auto ScenePanel::SetGizmoMode( imgui::GizmoMode mode ) -> void {
        mGizmoMode = mode;
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

    auto ScenePanel::DrawFinalImage() -> void {
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

    auto ScenePanel::DrawSceneButtons() -> void {
        constexpr float buttonCount{ 3.0f };
        constexpr float paddingVertical{ 3.0f };

        const ImVec2 buttonSize{35.f, 25.f};
        const ImVec2 groupSize{buttonSize.x * buttonCount, buttonSize.y + paddingVertical};

        ImGui::SetCursorPos({mViewportWidth * 0.5f - (groupSize.x * 0.5f), mStartCursorPos.y + paddingVertical});
        ImGui::BeginGroup();
        {
            ImGuiScopedStyleVar frameBorderSize( ImGuiStyleVar_FrameBorderSize, 0.0f );
            ImGuiScopedStyleVar itemSpacing( ImGuiStyleVar_ItemSpacing, ImVec2{1, 1} );
            ImGuiScopedStyleVar frameRounding( ImGuiStyleVar_FrameRounding, 1.0f );

            const bool isSceneSimulating{ mEditorState->mActiveScene->IsSimulating() };
            ImGui::BeginDisabled(isSceneSimulating);
            eastl::string simulateButtonLabel{ string::Format( "{}##SceneSimulateButtonID", ICON_MD_BRUSH ) };
            if (ImGui::Button(simulateButtonLabel.c_str(), buttonSize)) {
                mEditorState->mActiveScene->SetState( SceneState::eSimulating );
            }
            SetCursorHandOnLastItemHovered();

            ImGui::EndDisabled();

            ImGui::SameLine();

            const bool isScenePlaying{ mEditorState->mActiveScene->IsPlaying() };

            ImGui::BeginDisabled(isScenePlaying);
            eastl::string playButtonLabel{ string::Format( "{}##ScenePlayButtonID",
                isScenePlaying ? ICON_MD_PAUSE : ICON_MD_PLAY_ARROW ) }; // TODO: remove pause option?
            if (ImGui::Button(playButtonLabel.c_str(), buttonSize)) {
                mEditorState->mActiveScene->SetState( SceneState::ePlaying );
            }
            SetCursorHandOnLastItemHovered();

            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!isScenePlaying && !isSceneSimulating);
            eastl::string stopButtonLabel{ string::Format( "{}##SceneStopButtonID", ICON_MD_STOP ) };
            if (ImGui::Button(stopButtonLabel.c_str(), buttonSize)) {
                mEditorState->mActiveScene->SetState( SceneState::eIdle );
            }
            SetCursorHandOnLastItemHovered();

            ImGui::EndDisabled();
        }

        ImGui::EndGroup();
    }

    auto ScenePanel::DrawTransformGizmos() -> void {
        const float frameHeight{ 1.3f * ImGui::GetFrameHeight() };
        const ImVec2 framePadding{ ImGui::GetStyle().FramePadding };
        const ImVec2 buttonSize{ frameHeight, frameHeight };
        constexpr float buttonCount{ 9.0f };
        const ImVec2 windowPos{ ImGui::GetWindowPos() };
        const ImVec2 contentMin{ ImGui::GetWindowContentRegionMin() };
        const ImVec2 panelTopLeft{ windowPos.x + contentMin.x, windowPos.y + contentMin.y };

        const ImVec2 gizmoPos{ panelTopLeft.x + mGizmoPosition.x, panelTopLeft.y + mGizmoPosition.y };
        const ImRect bb(
                gizmoPos.x,
                gizmoPos.y,
                gizmoPos.x + buttonSize.x + 8,
                gizmoPos.y + ( buttonSize.y + 2 ) * ( buttonCount + 0.5f ) );
        ImVec4 frameColor{ ImGui::GetStyleColorVec4( ImGuiCol_Tab ) };
        frameColor.w = 0.5f;
        ImGui::RenderFrame( bb.Min, bb.Max, ImGui::GetColorU32( frameColor ), false, ImGui::GetStyle().FrameRounding );

        const auto tempGizmoPosition = mGizmoPosition;
        ImGui::SetCursorPos(
                { mStartCursorPos.x + tempGizmoPosition.x + framePadding.x, mStartCursorPos.y + tempGizmoPosition.y } );
        ImGui::BeginGroup();
        {
            ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
            ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 1, 1 } );

            const ImVec2 draggerCursorPos{ ImGui::GetCursorPos() };
            ImGui::SetCursorPosX( draggerCursorPos.x + framePadding.x );
            const eastl::string iconDots{ GetStringFromUnicode( 57952 ) };
            ImGui::TextUnformatted( iconDots.c_str() );
            ImVec2 draggerSize{ ImGui::CalcTextSize( iconDots.c_str() ) };
            draggerSize.x *= 2.0f;
            ImGui::SetCursorPos( draggerCursorPos );
            ImGui::InvisibleButton( "GizmoDragger", draggerSize );
            static ImVec2 lastMousePosition{ ImGui::GetMousePos() };
            const ImVec2 mousePos{ ImGui::GetMousePos() };
            if ( ImGui::IsItemActive() ) {
                mGizmoPosition.x += mousePos.x - lastMousePosition.x;
                mGizmoPosition.y += mousePos.y - lastMousePosition.y;
            }
            lastMousePosition = mousePos;

            ImGui::Spacing();
            ImGui::Spacing();

            constexpr f32 alpha{ 0.6f };
            constexpr bool handCursorOnHover{ true };
            const eastl::string iconTranslate{ GetStringFromUnicode( 57523 ) };
            if ( widget::ToggleButton( iconTranslate.c_str(), mGizmoType == GizmoType::eTranslation, handCursorOnHover, buttonSize, alpha, alpha ) ) {
                mGizmoType = GizmoType::eTranslation;
            }
            ImGui::Spacing();
            ImGui::Spacing();

            const eastl::string iconRotate{ GetStringFromUnicode( 58423 ) };
            if ( widget::ToggleButton( iconRotate.c_str(), mGizmoType == GizmoType::eRotation, handCursorOnHover, buttonSize, alpha, alpha ) ) {
                mGizmoType = GizmoType::eRotation;
            }
            ImGui::Spacing();
            ImGui::Spacing();

            const eastl::string iconScale{ GetStringFromUnicode( 58165 ) };
            if ( widget::ToggleButton( iconScale.c_str(), mGizmoType == GizmoType::eScale, handCursorOnHover, buttonSize, alpha, alpha ) ) {
                mGizmoType = GizmoType::eScale;
            }
            ImGui::Spacing();
            ImGui::Spacing();

            const eastl::string iconBounds{ GetStringFromUnicode( 59902 ) };
            if ( widget::ToggleButton( iconBounds.c_str(), mGizmoType == GizmoType::eBounds, handCursorOnHover, buttonSize, alpha, alpha ) ) {
                mGizmoType = GizmoType::eBounds;
            }
            ImGui::Spacing();
            ImGui::Spacing();

            // Gizmo mode (local scape or world)
            const eastl::string iconGizmoMode{ GetStringFromUnicode( mGizmoMode == GizmoMode::eWorld ? 58832 : 58833 ) };
            if ( widget::ToggleButton( iconGizmoMode.c_str(), false, handCursorOnHover, buttonSize, alpha, alpha ) ) {
                if ( mGizmoMode == GizmoMode::eWorld ) {
                    mGizmoMode = GizmoMode::eLocal;
                } else {
                    mGizmoMode = GizmoMode::eWorld;
                }
            }
            ImGui::Spacing();
            ImGui::Spacing();

            eastl::string iconToggleCameraProjection{};
            if (mEditorState->mActiveCamera->IsOrthographic()) {
                iconToggleCameraProjection = GetStringFromUnicode( 58301 ); // Orthographic icon
            } else {
                iconToggleCameraProjection = GetStringFromUnicode( 58381 ); // Perspective icon
            }
            if ( widget::ToggleButton( iconToggleCameraProjection.c_str(), false , handCursorOnHover, buttonSize, alpha, alpha ) ) {
                if ( mEditorState->mActiveCamera->IsOrthographic() ) {
                    mEditorState->mActiveCamera->SetProjectionType( ProjectionType::eOrthographic );
                } else {
                    mEditorState->mActiveCamera->SetProjectionType( ProjectionType::ePerspective );
                }
            }
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::PopStyleVar( 2 );
        }

        ImGui::EndGroup();
    }

    auto ScenePanel::DrawManipulationGizmos() -> void {
        Entity *currentSelection{ mEditorState->mSelectedEntity };
        if (currentSelection == nullptr || !currentSelection->IsValid() || !currentSelection->GetComponent<TagComponent>().IsActive()) {
            return;
        }

        TransformComponent &transformComponent{ currentSelection->GetComponent<TransformComponent>() };

        const float4x4 &cameraView{ mEditorState->mActiveCamera->GetViewMatrix() };
        const float4x4 &cameraProjection{ mEditorState->mActiveCamera->GetProjection() };

        float4x4 objectTransform{ transformComponent.GetTransform() };

        ImGuizmo::OPERATION operation{ InferManipulationMode( mGizmoType ) };
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

            auto makeTool = [&]( const char *icon, GizmoType type ) {
                const bool active{
                    mGizmoType == as<imgui::GizmoType>( type )
                };

                const ImVec2 btnSize{ 28.0f, 28.0f };
                const ImVec2 iconPadding{ 2.0f, 2.0f };

                if (active) {
                    ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.75f, 0.75f, 0.75f, 0.85f } );
                }

                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, iconPadding );

                if (ImGui::Button( icon, btnSize )) {
                    mGizmoType = as<imgui::GizmoType>( type );
                }

                ImGui::PopStyleVar();

                if (active) ImGui::PopStyleColor();

                ImGui::SameLine();
            };

            // Extra spacing on first button
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 6.0f, 0.0f } );
            makeTool( ICON_MD_OPEN_WITH, imgui::GizmoType::eTranslation );
            ImGui::PopStyleVar();

            makeTool( ICON_MD_ROTATE_RIGHT, imgui::GizmoType::eRotation );
            makeTool( ICON_MD_OPEN_IN_FULL, imgui::GizmoType::eScale );

            ImGui::NewLine();
        }

        ImGui::End();

        ImGui::PopStyleVar( 3 );
        ImGui::PopStyleColor( 2 );
    }

}// namespace Mikoto