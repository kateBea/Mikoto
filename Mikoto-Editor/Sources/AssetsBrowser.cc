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

#include <EASTL/sort.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/AssetsBrowser.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace core;
    using namespace asset;
    using namespace filesystem;
    using namespace renderer;
    using namespace renderer::rhi;


    static auto PrefabName( PrefabModelType type ) -> cstr {
        switch ( type ) {
            case PrefabModelType::eCube:
                return MKT_LOC( "prefabs_browser_cube" ).c_str();
            case PrefabModelType::eSphere:
                return MKT_LOC( "prefabs_browser_sphere" ).c_str();
            case PrefabModelType::eCone:
                return MKT_LOC( "prefabs_browser_cone" ).c_str();
            case PrefabModelType::eCylinder:
                return MKT_LOC( "prefabs_browser_cylinder" ).c_str();
        }

        return MKT_LOC( "prefabs_browser_prefab" ).c_str();
    }


    MaterialsBrowserPanel::MaterialsBrowserPanel( const MaterialsBrowserPanelCreateInfo& info )
        : Panel{ "Materials Browser", "Panel", info.mGroup, info.mIsToggleable },
          mEditorState{ info.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_PALETTE, mPanelName );
        mPanelIsVisible = false;
    }

    auto MaterialsBrowserPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            // Placeholder until the material browser's data source and interactions are defined.
            ImGui::TextDisabled( MKT_LOC( "materials_browser_materials_browser" ).c_str() );
            ImGui::TextWrapped( MKT_LOC( "materials_browser_material_tracking_is_not_implemented_yet" ).c_str() );
        }

        ImGui::End();
    }

    AnimationsBrowserPanel::AnimationsBrowserPanel( const AnimationsBrowserPanelCreateInfo& info )
        : Panel{ "Animations Browser", "Panel", info.mGroup, info.mIsToggleable },
          mEditorState{ info.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_MOVIE, mPanelName );
        mPanelIsVisible = false;
    }

    auto AnimationsBrowserPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            // Placeholder until the editor exposes its animation collection.
            ImGui::TextDisabled( MKT_LOC( "animations_browser_animations_browser" ).c_str() );
            ImGui::TextWrapped( MKT_LOC( "animations_browser_animation_tracking_is_not_implemented_yet" ).c_str() );
        }

        ImGui::End();
    }

    PrefabsBrowserPanel::PrefabsBrowserPanel( const PrefabsBrowserPanelCreateInfo& info )
        : Panel{ "Prefabs Browser", "Panel", info.mGroup, info.mIsToggleable },
          mEditorState{ info.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_VIEW_IN_AR, mPanelName );
        mPanelIsVisible = false;
    }

    auto PrefabsBrowserPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( ImVec2{ 540.0f, 400.0f }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            ImGui::End();
            return;
        }

        mPanelIsHovered = ImGui::IsWindowHovered();
        mPanelIsFocused = ImGui::IsWindowFocused();

        if ( !mEditorState ) {
            ImGui::TextDisabled( MKT_LOC( "prefabs_browser_no_editor_state_available" ).c_str() );
            ImGui::End();
            return;
        }

        // Allocate the cache only when opened. Queue previews asynchronously
        // through the same renderer used by the other preview consumers.
        if ( !mThumbnailCache && mEditorState->mThumbnailRenderer ) {
            mThumbnailCache = eastl::make_unique<ThumbnailCache>(
                mEditorState->mThumbnailRenderer, renderer::RenderSystem::Get()->GetGpuDevice() );
        }

        if ( mThumbnailCache ) {
            mThumbnailCache->UploadReady();
        }

        ImGui::TextUnformatted( MKT_LOC( "prefabs_browser_thumbnail_size" ).c_str() );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( -1.0f );
        ImGui::SliderFloat( "##ThumbnailSize", &mThumbnailSize, 48.0f, 256.0f, "%.0f px", ImGuiSliderFlags_AlwaysClamp );

        ImGui::SetNextItemWidth( -1.0f );
        const auto filterHint{ string::Format( "{}{}", ICON_MD_SEARCH, MKT_LOC( "prefabs_browser_filter_prefabs" ).c_str() ) };

        if ( ImGui::InputTextWithHint( "##Filter", filterHint.c_str(), mFilter.InputBuf, IM_ARRAYSIZE( mFilter.InputBuf ) ) ) {
            mFilter.Build();
        }

        // Resolve selection against the live collection rather than retaining
        // an iterator across editor-state changes.
        const auto& prefabs{ mEditorState->mPrefabs };
        const auto selected{ prefabs.find( static_cast<PrefabModelType>( mSelectedPrefab ) ) };
        const filesystem::Path path{ selected != prefabs.end() ? selected->second.mReference.GetPath() : filesystem::Path{} };
        eastl::string pathText{ path.IsEmpty() ? "Resources/Prefabs" : path.GetC_Str() };

        ImGui::TextUnformatted( MKT_LOC( "prefabs_browser_path" ).c_str() );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( -1.0f );
        ImGui::InputText( "##PrefabPath", pathText.data(), pathText.size() + 1, ImGuiInputTextFlags_ReadOnly );
        mikoto::imgui::widget::ItemTooltip( "%s", pathText.c_str() );
        ImGui::Separator();

        eastl::vector<PrefabModelType> filtered{};
        filtered.reserve( prefabs.size() );

        for ( const auto& prefabEntry: prefabs ) {
            const auto& type{ prefabEntry.first };

            if ( mFilter.PassFilter( PrefabName( type ) ) ) {
                filtered.push_back( type );
            }
        }

        eastl::sort( filtered.begin(), filtered.end(), []( PrefabModelType lhs, PrefabModelType rhs ) {
            return eastl::string_view{ PrefabName( lhs ) } < eastl::string_view{ PrefabName( rhs ) };
        } );

        if ( ImGui::BeginChild( "PrefabGrid", ImVec2{ 0.0f, 0.0f } ) ) {
            if ( filtered.empty() ) {
                ImGui::TextDisabled( mFilter.IsActive() ? MKT_LOC( "prefabs_browser_no_matching_prefabs" ).c_str() : MKT_LOC( "prefabs_browser_no_prefabs_loaded" ).c_str() );
            }
            else {
                const auto& style{ ImGui::GetStyle() };
                const core::f32 availableWidth{ ImGui::GetContentRegionAvail().x };
                const core::f32 padding{ 2.0f * ( style.FramePadding.x + style.CellPadding.x ) };
                const core::i32 columns{ eastl::clamp( static_cast<core::i32>( availableWidth / ( mThumbnailSize + padding + 12.0f ) ), 1, 64 ) };
                const core::f32 size{ eastl::max( 1.0f, eastl::min( mThumbnailSize, availableWidth / columns - padding ) ) };

                if ( ImGui::BeginTable( "Prefabs", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings ) ) {
                    for ( const auto type: filtered ) {
                        ImGui::TableNextColumn();
                        ImGui::PushID( static_cast<core::i32>( type ) );

                        const auto& prefab{ prefabs.at( type ) };
                        const auto& modelPath{ prefab.mReference.GetPath() };
                        renderer::rhi::TextureHandle thumbnail{};

                        if ( mThumbnailCache && prefab.mRequest.GetState() == asset::AssetState::eReady && !modelPath.IsEmpty() ) {
                            if ( !mThumbnailCache->Contains( modelPath ) ) {
                                mThumbnailCache->CreateThumbnailAsync( modelPath );
                            }

                            thumbnail = mThumbnailCache->GetThumbnail( modelPath ).mThumbnail;
                        }

                        // Register the texture every frame; cached ImGui IDs may
                        // be retired while this panel is hidden.
                        const ImTextureID image{ thumbnail ? imgui::ImGuiService::Get()->GetTextureID( thumbnail ) : ImTextureID{} };
                        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
                        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{} );
                        const auto tint{ ImGui::GetStyleColorVec4( ImGuiCol_Text ) };
                        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4{ tint.x, tint.y, tint.z, 0.035f } );
                        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4{ tint.x, tint.y, tint.z, 0.06f } );

                        if ( image != ImTextureID{} ) {
                            imgui::widget::FadingImageButton( "##Prefab", image, ImVec2{ size, size } );
                        }
                        else {
                            ImGui::Button( ICON_MD_VIEW_IN_AR, ImVec2{ size + 2.0f * style.FramePadding.x, size + 2.0f * style.FramePadding.y } );
                        }

                        ImGui::PopStyleColor( 3 );
                        ImGui::PopStyleVar();

                        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) || ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
                            mSelectedPrefab = static_cast<core::i32>( type );
                        }

                        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "prefabs_browser_drag_into_the_scene_panel_to_add_an_instance" ).c_str(), PrefabName( type ), modelPath.GetC_Str() );

                        if ( ImGui::IsItemHovered() ) {
                            if ( prefab.mRequest.GetState() == asset::AssetState::eFailed ) {
                                ImGui::SetTooltip( "%s", prefab.mRequest.GetError().c_str() );
                            }
                            else if ( prefab.mRequest.GetState() != asset::AssetState::eReady ) {
                                ImGui::SetTooltip( "%s: loading...", PrefabName( type ) );
                            }
                        }

                        if ( !modelPath.IsEmpty() && ImGui::BeginDragDropSource() ) {
                            const auto text{ modelPath.GetPath() };
                            ImGui::SetDragDropPayload( "CONTENT_BROWSER_TEXT_MODEL", text.data(), text.size() + 1 );

                            if ( image != ImTextureID{} ) {
                                ImGui::Image( image, ImVec2{ 48.0f, 48.0f } );
                            }

                            ImGui::TextUnformatted( PrefabName( type ) );
                            ImGui::EndDragDropSource();
                        }

                        if ( ImGui::BeginPopupContextItem( "PrefabActions" ) ) {
                            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "prefabs_browser_copy_path", "Copy path" ).c_str(), nullptr, false, !modelPath.IsEmpty() ) ) {
                                ImGui::SetClipboardText( modelPath.GetC_Str() );
                            }

                            ImGui::EndPopup();
                        }

                        const core::f32 labelWidth{ size + 2.0f * style.FramePadding.x };
                        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{} );
                        ImGui::PushStyleVar( ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f } );
                        imgui::CenteredText( PrefabName( type ), labelWidth, ImGui::GetTextLineHeight() );
                        ImGui::PopStyleVar( 2 );
                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }
            }
        }

        ImGui::EndChild();
        ImGui::End();
    }

}// namespace mikoto::editor
