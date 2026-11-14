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

#include <glm/glm.hpp>

#include <Core/LocalizationService.hh>

#include <Renderer/Interface/CanvasInput.hh>

#include <Scripting/ScriptingService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiService.hh>

#include <Panels/GamePreviewPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace core;
    using namespace scene;
    using namespace renderer;
    using namespace renderer::rhi;

    GamePreviewPanel::GamePreviewPanel( const GamePreviewPanelCreateInfo& description )
        : Panel{ "Game", "Game", description.mGroup, description.mIsToggleable }, mState{ description.mState } {
        mPanelIsVisible = false;
    }

    auto GamePreviewPanel::OnRender( float ) -> void {
        mState->mGamePreviewRequested = false;
        mPanelIsFocused = false;
        mPanelIsHovered = false;

        if ( !mPanelIsVisible ) {
            if ( mState->mActiveScene ) {
                renderer::CancelCanvasInput( *mState->mActiveScene );
            }

            return;
        }

        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );
        const bool visible{ imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) };
        ImGui::PopStyleVar();
        mPanelIsFocused = visible && mPanelIsVisible && ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );
        mPanelIsHovered = visible && mPanelIsVisible && ImGui::IsWindowHovered();

        if ( visible && mPanelIsVisible ) {
            if ( !mState->mGameRenderer ) {
                ImGui::TextWrapped( MKT_LOC( "game_preview_game_scene_rendering_is_unavailable_for_the_current_backend" ).c_str() );
            }
            else if ( !mState->mGameCamera ) {
                const auto message{ MKT_LOC( "game_preview_no_active_main_camera_add_a_camera_component_and_mark" ) };
                const auto available{ ImGui::GetContentRegionAvail() };
                imgui::CenteredText( message.c_str(), available.x, available.y, true );
            }
            else {
                const ImVec2 available{ ImGui::GetContentRegionAvail() };

                if ( available.x > 1.0f && available.y > 1.0f ) {
                    mState->mGamePreviewRequested = true;
                    mState->mGamePreviewSize = { available.x, available.y };
                    ImVec2 size{ available };

                    if ( mState->mGameCameraFixedAspect ) {
                        // An explicitly locked camera lens retains its framing.
                        const float aspect{ glm::max( mState->mGameCamera->GetAspectRatio(), 0.001f ) };
                        const float height{ glm::min( available.y, available.x / aspect ) };
                        size = { height * aspect, height };
                    }

                    const ImVec2 cursor{ ImGui::GetCursorPos() };
                    ImGui::SetCursorPos( { cursor.x + ( available.x - size.x ) * 0.5f, cursor.y + ( available.y - size.y ) * 0.5f } );
                    auto texture{ mState->mGameRenderer->GetFinalImage() };

                    if ( texture ) {
                        ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( texture ), size );
                        const bool hovered{ ImGui::IsItemHovered() };

                        if ( hovered && ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ||
                                            ImGui::IsMouseClicked( ImGuiMouseButton_Right ) || ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) ) ) {
                            ImGui::SetWindowFocus();
                        }

                        mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );
                        scripting::ScriptingService::Get()->SetInputFocus(
                            mState->mRuntimeActive && mPanelIsFocused && !ImGui::GetIO().WantTextInput, hovered );

                        if ( mState->mRuntimeActive && mState->mActiveScene && ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) ) {
                            const auto origin = ImGui::GetItemRectMin();
                            const auto mouse = ImGui::GetMousePos();
                            renderer::UpdateCanvasInput( *mState->mActiveScene, *mState->mGameCamera,
                                { ( mouse.x - origin.x ) / size.x, ( mouse.y - origin.y ) / size.y },
                                ImGui::IsMouseDown( ImGuiMouseButton_Left ), ImGui::IsMouseClicked( ImGuiMouseButton_Left ),
                                ImGui::IsMouseReleased( ImGuiMouseButton_Left ), ImGui::IsItemHovered() );
                            // The Game window owns keyboard navigation only while
                            // focused; editing inspector text must not activate UI.
                            if ( !ImGui::GetIO().WantTextInput ) {
                                const auto navigate = [&]( ImGuiKey key, renderer::CanvasNavigation action ) {
                                    if ( ImGui::IsKeyPressed( key, false ) ) {
                                        renderer::NavigateCanvasInput( *mState->mActiveScene, action );
                                    }
                                };
                                navigate( ImGuiKey_Tab, ImGui::GetIO().KeyShift ? renderer::CanvasNavigation::ePrevious : renderer::CanvasNavigation::eNext );
                                navigate( ImGuiKey_Enter, renderer::CanvasNavigation::eActivate );
                                navigate( ImGuiKey_Space, renderer::CanvasNavigation::eActivate );
                                navigate( ImGuiKey_LeftArrow, renderer::CanvasNavigation::eDecrease );
                                navigate( ImGuiKey_RightArrow, renderer::CanvasNavigation::eIncrease );
                                navigate( ImGuiKey_Home, renderer::CanvasNavigation::eHome );
                                navigate( ImGuiKey_End, renderer::CanvasNavigation::eEnd );
                            }
                        }
                        else if ( mState->mActiveScene ) {
                            renderer::CancelCanvasInput( *mState->mActiveScene );
                        }
                    }
                    else {
                        ImGui::TextUnformatted( MKT_LOC( "game_preview_preparing_game_preview" ).c_str() );
                    }
                }
            }
        }

        if ( ( !mState->mGamePreviewRequested || !mState->mRuntimeActive ) && mState->mActiveScene ) {
            renderer::CancelCanvasInput( *mState->mActiveScene );
        }

        ImGui::End();
    }

}// namespace mikoto::editor
