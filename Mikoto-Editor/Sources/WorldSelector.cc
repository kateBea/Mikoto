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

#include <exception>
#include <filesystem>

#include <imgui.h>

#include <EASTL/utility.h>
#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/ImageProcessing.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/WorldSelector.hh>

namespace mikoto::editor {

    using renderer::rhi::IGpuDevice;
    using renderer::rhi::TextureHandle;

    static constexpr core::cstr kDemoProjectsPath{ "Resources/Demo Projects" };
    static constexpr core::f32 kPreviewAspectRatio{ 16.0f / 9.0f };
    static constexpr core::f32 kCardPreferredWidth{ 200.0f };

    MKT_NODISCARD static auto CreatePreviewTexture( IGpuDevice* device, const asset::ImageHandle& image ) -> TextureHandle {
        if ( !device || !image || image->mFormat != asset::ImageFormat::eRGBA8_UINT ) {
            return {};
        }

        using namespace renderer::rhi;

        // Use the same raw UI image upload as the editor title bar (EditorLayer::InitTitleBar).
        return device->CreateTexture( TextureCreateDescription{}
            .SetName( "World selector preview" )
            .SetImageData( image )
            .SetWidth( image->mWidth )
            .SetHeight( image->mHeight )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::ShaderResource |
                TextureUsageFlagsBits::CopyDest )
            .SetFormat( Format::eRGBA8_UNORM ) );
    }

    static auto DrawWorkspaceBackdrop( ImVec2 workspaceMin, ImVec2 workspaceSize ) -> void {
        const ImVec2 workspaceMax{ workspaceMin.x + workspaceSize.x, workspaceMin.y + workspaceSize.y };
        const ImVec2 windowPosition{ ImGui::GetWindowPos() };
        const ImVec2 windowSize{ ImGui::GetWindowSize() };
        const ImVec2 windowMin{
            eastl::clamp( windowPosition.x, workspaceMin.x, workspaceMax.x ),
            eastl::clamp( windowPosition.y, workspaceMin.y, workspaceMax.y )
        };
        const ImVec2 windowMax{
            eastl::clamp( windowPosition.x + windowSize.x, workspaceMin.x, workspaceMax.x ),
            eastl::clamp( windowPosition.y + windowSize.y, workspaceMin.y, workspaceMax.y )
        };

        // Draw with the selector so menus and popups retain their normal stacking order.
        auto* drawList{ ImGui::GetWindowDrawList() };
        const ImU32 tint{ ImGui::GetColorU32( ImGuiCol_ModalWindowDimBg ) };
        drawList->PushClipRect( workspaceMin, workspaceMax, false );
        drawList->AddRectFilled( workspaceMin, { workspaceMax.x, windowMin.y }, tint );
        drawList->AddRectFilled( { workspaceMin.x, windowMax.y }, workspaceMax, tint );
        drawList->AddRectFilled( { workspaceMin.x, windowMin.y }, { windowMin.x, windowMax.y }, tint );
        drawList->AddRectFilled( { windowMax.x, windowMin.y }, { workspaceMax.x, windowMax.y }, tint );
        drawList->PopClipRect();
    }

    static auto DrawPreview( ImDrawList* drawList, ImVec2 min, ImVec2 max, ImTextureID image,
        const TextureHandle& texture, core::f32 rounding ) -> void {
        if ( image ) {
            // Center the image in the preview and crop only the excess along its longer axis.
            const core::f32 imageAspect{ core::as<core::f32>( texture->GetWidth() ) / texture->GetHeight() };
            const core::f32 previewAspect{ ( max.x - min.x ) / ( max.y - min.y ) };
            const core::f32 cropX{ eastl::min( 1.0f, previewAspect / imageAspect ) };
            const core::f32 cropY{ eastl::min( 1.0f, imageAspect / previewAspect ) };
            const ImVec2 uvMin{ ( 1.0f - cropX ) * 0.5f, ( 1.0f - cropY ) * 0.5f };
            const ImVec2 uvMax{ 1.0f - uvMin.x, 1.0f - uvMin.y };

            drawList->AddImageRounded( image, min, max, uvMin, uvMax, ImGui::GetColorU32( IM_COL32_WHITE ), rounding );
            return;
        }

        const ImVec2 iconSize{ ImGui::CalcTextSize( ICON_MD_LANDSCAPE ) };
        const ImVec2 iconPosition{ ( min.x + max.x - iconSize.x ) * 0.5f, ( min.y + max.y - iconSize.y ) * 0.5f };

        drawList->AddRectFilled( min, max, ImGui::GetColorU32( ImGuiCol_FrameBg ), rounding );
        drawList->AddText( iconPosition, ImGui::GetColorU32( ImGuiCol_TextDisabled ), ICON_MD_LANDSCAPE );
    }

    auto WorldSelector::Open() -> void {
        mSearch.Clear();
        mSelectedProject = {};
        Refresh();
        mVisible = true;
    }

    auto WorldSelector::Close() -> void {
        mVisible = false;
    }

    auto WorldSelector::IsOpen() const -> bool {
        return mVisible;
    }

    auto WorldSelector::FindDemoProjects() -> eastl::vector<filesystem::Path> {
        namespace fs = std::filesystem;

        std::error_code rootError{};
        constexpr auto options{ fs::directory_options::skip_permission_denied };
        eastl::vector<filesystem::Path> projects{};

        for ( fs::directory_iterator directory{ kDemoProjectsPath, options, rootError }, end;
              !rootError && directory != end; directory.increment( rootError ) ) {
            std::error_code typeError{};
            if ( !directory->is_directory( typeError ) ) {
                continue;
            }

            // A missing or inaccessible demo shouldn't hide the other projects.
            std::error_code demoError{};
            for ( fs::directory_iterator file{ directory->path(), options, demoError }, last;
                  !demoError && file != last; file.increment( demoError ) ) {
                if ( file->is_regular_file( typeError ) && file->path().extension() == ".mktproj" ) {
                    projects.emplace_back( file->path() );
                }
            }
        }

        eastl::sort( projects.begin(), projects.end(), []( const auto& left, const auto& right ) {
            return left.GetStem() == right.GetStem() ? left < right : left.GetStem() < right.GetStem();
        } );

        return projects;
    }

    auto WorldSelector::Refresh() -> void {
        auto projects{ FindDemoProjects() };
        mWorlds.clear();
        mWorlds.reserve( projects.size() );

        for ( auto& project: projects ) {
            const eastl::string stem{ project.GetStem() };
            auto previewPath{ project.GetPathTyped<std::filesystem::path>() };
            previewPath.replace_extension( ".preview.png" );

            World world{
                .mProject = eastl::move( project ),
                .mPreviewPath = filesystem::Path{ previewPath },
                .mFallbackTitle = stem,
                .mTitleKey = "world_demo_" + stem,
                .mDescriptionKey = "world_demo_" + stem + "_description"
            };
            eastl::replace( world.mFallbackTitle.begin(), world.mFallbackTitle.end(), '_', ' ' );
            mWorlds.push_back( eastl::move( world ) );
        }

        if ( !FindSelection() ) {
            mSelectedProject = {};
        }
    }

    auto WorldSelector::Draw( IGpuDevice* device, ImVec2 workspacePosition, ImVec2 workspaceSize,
        bool dimWorkspace ) -> filesystem::Path {
        MKT_BEGIN_PROFILER();

        if ( !mVisible ) {
            // Release previews on the next frame, after ImGui has used their texture IDs.
            mWorlds.clear();
            return {};
        }

        constexpr ImVec2 preferredSize{ 1440.0f, 880.0f };
        constexpr core::f32 workspaceMargin{ 32.0f };
        const ImVec2 size{
            eastl::min( preferredSize.x, eastl::max( 1.0f, workspaceSize.x - workspaceMargin ) ),
            eastl::min( preferredSize.y, eastl::max( 1.0f, workspaceSize.y - workspaceMargin ) )
        };
        const ImVec2 center{ workspacePosition.x + workspaceSize.x * 0.5f, workspacePosition.y + workspaceSize.y * 0.5f };

        // A regular editor window keeps the menu and caption controls interactive.
        ImGui::SetNextWindowViewport( ImGui::GetMainViewport()->ID );
        ImGui::SetNextWindowPos( center, ImGuiCond_Always, { 0.5f, 0.5f } );
        ImGui::SetNextWindowSize( size, ImGuiCond_Always );

        constexpr ImGuiWindowFlags flags{
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse
        };

        const auto title{ MKT_LOC( "world_selector_title" ) + "###WorldSelector" };
        if ( !imgui::BeginWindow( title.c_str(), &mVisible, flags ) || !mVisible ) {
            ImGui::End();
            return {};
        }

        if ( dimWorkspace && workspaceSize.x > 0.0f && workspaceSize.y > 0.0f ) {
            DrawWorkspaceBackdrop( workspacePosition, workspaceSize );
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "world_selector_hint" ).c_str() );
        DrawSearch();
        ImGui::Spacing();

        const auto& style{ ImGui::GetStyle() };
        const core::f32 availableWidth{ eastl::max( 1.0f, ImGui::GetContentRegionAvail().x - style.ItemSpacing.x ) };
        const core::f32 detailsWidth{ eastl::min( eastl::max( availableWidth * 0.29f, 240.0f ), availableWidth * 0.47f ) };

        auto project{ DrawGrid( device, availableWidth - detailsWidth ) };
        ImGui::SameLine();
        auto detailProject{ DrawDetails( device ) };

        if ( project.IsEmpty() ) {
            project = eastl::move( detailProject );
        }

        const bool closeWithEscape{ ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) &&
            !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed( ImGuiKey_Escape ) };
        if ( !project.IsEmpty() || closeWithEscape ) {
            Close();
        }

        ImGui::End();
        return project;
    }

    auto WorldSelector::DrawSearch() -> void {
        const auto& style{ ImGui::GetStyle() };
        const auto& refresh{ MKT_LOC( "world_selector_refresh" ) };
        const auto count{ string::Format( "{}: {}", MKT_LOC( "world_selector_count" ).c_str(), mWorlds.size() ) };

        const core::f32 countWidth{ ImGui::CalcTextSize( count.c_str() ).x };
        const core::f32 refreshWidth{ ImGui::CalcTextSize( refresh.c_str() ).x + style.FramePadding.x * 2.0f };
        const core::f32 searchWidth{ ImGui::GetContentRegionAvail().x - refreshWidth - countWidth - style.ItemSpacing.x * 2.0f };

        ImGui::SetNextItemWidth( eastl::max( ImGui::GetFrameHeight(), searchWidth ) );
        if ( ImGui::InputTextWithHint( "##WorldSearch", MKT_LOC( "world_selector_search" ).c_str(),
                 mSearch.InputBuf, IM_ARRAYSIZE( mSearch.InputBuf ) ) ) {
            mSearch.Build();

            if ( !FindSelection() ) {
                mSelectedProject = {};
            }
        }

        ImGui::SameLine();
        
        if ( ImGui::Button( refresh.c_str() ) ) {
            Refresh();
        }

        imgui::SetCursorHandOnLastItemHovered();

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled( "%s", count.c_str() );
    }

    auto WorldSelector::DrawGrid( IGpuDevice* device, core::f32 width ) -> filesystem::Path {
        filesystem::Path openProject{};

        if ( ImGui::BeginChild( "##WorldGrid", { width, 0.0f },
                 ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_AlwaysVerticalScrollbar ) ) {
            const core::f32 availableWidth{ eastl::max( 1.0f, ImGui::GetContentRegionAvail().x ) };
            const core::f32 spacing{ ImGui::GetStyle().ItemSpacing.x };
            const core::i32 columns{ eastl::max( 1, core::as<core::i32>( ( availableWidth + spacing ) / ( kCardPreferredWidth + spacing ) ) ) };
            const core::f32 cardWidth{ ( availableWidth + spacing ) / columns - spacing };
            core::i32 visible{};

            for ( auto& world: mWorlds ) {
                if ( !PassesFilter( world ) ) {
                    continue;
                }

                if ( visible % columns != 0 ) {
                    ImGui::SameLine();
                }

                if ( DrawCard( world, device, cardWidth ) ) {
                    openProject = world.mProject;
                }

                ++visible;
            }

            if ( visible == 0 ) {
                ImGui::TextDisabled( "%s", MKT_LOC( mWorlds.empty() ? "world_selector_empty" : "world_selector_no_matches" ).c_str() );
            }
        }

        ImGui::EndChild();
        return openProject;
    }

    auto WorldSelector::DrawCard( World& world, IGpuDevice* device, core::f32 width ) -> bool {
        constexpr core::f32 kCardInset{ 8.0f };
        constexpr core::f32 kCardRounding{ 5.0f };
        constexpr core::f32 kTitleLines{ 2.0f };

        const core::f32 imageWidth{ eastl::max( 1.0f, width - kCardInset * 2.0f ) };
        const core::f32 imageHeight{ imageWidth / kPreviewAspectRatio };
        const core::f32 cardHeight{ imageHeight + 3.0f * kCardInset + ImGui::GetTextLineHeight() * kTitleLines };

        ImGui::PushID( world.mProject.GetC_Str() );

        // One item owns the entire card. As in ImGui's Selectables demo, selection and double-click share its ID.
        // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/imgui_demo.cpp#L2317-L2340
        const bool selected{ mSelectedProject == world.mProject };
        ImGui::PushStyleColor( ImGuiCol_HeaderHovered, IM_COL32( 0, 0, 0, 0 ) );
        ImGui::PushStyleColor( ImGuiCol_HeaderActive, IM_COL32( 0, 0, 0, 0 ) );
        const bool pressed{ ImGui::Selectable( "##WorldCard", false, ImGuiSelectableFlags_AllowDoubleClick, { width, cardHeight } ) };
        ImGui::PopStyleColor( 2 );
        const bool openProject{ pressed && selected && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) };

        if ( pressed && !selected ) {
            mSelectedProject = world.mProject;
        }
        imgui::SetCursorHandOnLastItemHovered();

        if ( ImGui::IsItemVisible() ) {
            const auto& style{ ImGui::GetStyle() };
            const ImVec2 itemMin{ ImGui::GetItemRectMin() };
            const ImVec2 min{ itemMin.x + style.ItemSpacing.x * 0.5f, itemMin.y + style.ItemSpacing.y * 0.5f };
            const ImVec2 max{ min.x + width, min.y + cardHeight };
            const ImGuiCol background{ ( pressed || selected ) ? ImGuiCol_Header : ImGui::IsItemHovered() ? ImGuiCol_HeaderHovered : ImGuiCol_Button };
            auto* drawList{ ImGui::GetWindowDrawList() };

            drawList->AddRectFilled( min, max, ImGui::GetColorU32( background ), kCardRounding );
            drawList->AddRect( min, max, ImGui::GetColorU32( selected || pressed || ImGui::IsItemFocused() ? ImGuiCol_CheckMark : ImGuiCol_Border ), kCardRounding );
            drawList->PushClipRect( min, max, true );

            const ImVec2 imageMin{ min.x + kCardInset, min.y + kCardInset };
            const ImVec2 imageMax{ imageMin.x + imageWidth, imageMin.y + imageHeight };
            const ImTextureID preview{ GetPreviewID( world, device ) };
            DrawPreview( drawList, imageMin, imageMax, preview, world.mPreview, kCardRounding );
            drawList->AddText( nullptr, 0.0f, { imageMin.x, imageMax.y + kCardInset },
                ImGui::GetColorU32( ImGuiCol_Text ), GetTitle( world ).c_str(), nullptr, imageWidth );

            drawList->PopClipRect();
        }

        ImGui::PopID();
        return openProject;
    }

    auto WorldSelector::DrawDetails( IGpuDevice* device ) -> filesystem::Path {
        filesystem::Path openProject{};

        if ( ImGui::BeginChild( "##WorldDetails", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding ) ) {
            auto* world{ FindSelection() };
            constexpr core::f32 kFooterRows{ 2.0f };
            const core::f32 footerHeight{ ImGui::GetFrameHeightWithSpacing() * kFooterRows };

            if ( ImGui::BeginChild( "##WorldDetailsContent", { 0.0f, -footerHeight } ) ) {
                if ( world ) {
                    constexpr core::f32 kPreviewRounding{ 5.0f };
                    const core::f32 width{ eastl::max( 1.0f, ImGui::GetContentRegionAvail().x ) };
                    const ImVec2 size{ width, width / kPreviewAspectRatio };
                    const ImVec2 min{ ImGui::GetCursorScreenPos() };
                    const ImVec2 max{ min.x + size.x, min.y + size.y };

                    ImGui::Dummy( size );
                    if ( ImGui::IsItemVisible() ) {
                        const ImTextureID preview{ GetPreviewID( *world, device ) };
                        DrawPreview( ImGui::GetWindowDrawList(), min, max, preview, world->mPreview, kPreviewRounding );
                    }

                    ImGui::Spacing();
                    ImGui::TextWrapped( "%s", GetTitle( *world ).c_str() );
                    ImGui::TextWrapped( "%s", GetDescription( *world ).c_str() );

                    ImGui::Spacing();
                    ImGui::TextDisabled( "%s", MKT_LOC( "world_selector_location" ).c_str() );
                    ImGui::TextWrapped( "%s", world->mProject.GetC_Str() );
                } else {
                    ImGui::TextWrapped( "%s", MKT_LOC( "world_selector_select_hint" ).c_str() );
                }
            }

            ImGui::EndChild();

            ImGui::BeginDisabled( world == nullptr );
            if ( imgui::widget::AccentButton( MKT_LOC( "world_selector_open" ).c_str(), { ImGui::GetContentRegionAvail().x, 0.0f } ) && world ) {
                openProject = world->mProject;
            }

            imgui::SetCursorHandOnLastItemHovered();
            ImGui::EndDisabled();

            if ( ImGui::Button( MKT_LOC( "world_selector_back" ).c_str(), { ImGui::GetContentRegionAvail().x, 0.0f } ) ) {
                Close();
            }
            imgui::SetCursorHandOnLastItemHovered();
        }

        ImGui::EndChild();
        return openProject;
    }

    auto WorldSelector::FindSelection() -> World* {
        const auto found{ eastl::find_if( mWorlds.begin(), mWorlds.end(), [this]( const World& world ) {
            return world.mProject == mSelectedProject && PassesFilter( world );
        } ) };

        return found == mWorlds.end() ? nullptr : &*found;
    }

    auto WorldSelector::PassesFilter( const World& world ) const -> bool {
        return mSearch.PassFilter( GetTitle( world ).c_str() );
    }

    auto WorldSelector::GetTitle( const World& world ) -> const eastl::string& {
        const auto& title{ MKT_LOC( world.mTitleKey ) };
        return title == world.mTitleKey ? world.mFallbackTitle : title;
    }

    auto WorldSelector::GetDescription( const World& world ) -> const eastl::string& {
        const auto& description{ MKT_LOC( world.mDescriptionKey ) };

        // Keep newly added demos browsable before a description is added to the localization catalogs.
        static const eastl::string fallbackKey{ "world_selector_bundled_demo" };
        return description == world.mDescriptionKey ? MKT_LOC( fallbackKey ) : description;
    }

    auto WorldSelector::GetPreviewID( World& world, IGpuDevice* device ) -> ImTextureID {
        if ( !world.mPreviewChecked ) {
            world.mPreviewChecked = true;

            if ( world.mPreviewPath.IsFile() ) {
                try {
                    const auto image{ asset::ProcessImage2D( filesystem::FileHandle::New( world.mPreviewPath ) ) };
                    world.mPreview = CreatePreviewTexture( device, image );
                } catch ( const std::exception& error ) {
                    MKT_CORE_LOGGER_WARN( "Cannot load demo preview {}: {}", world.mPreviewPath.GetC_Str(), error.what() );
                }
            }
        }

        return world.mPreview ? imgui::ImGuiService::Get()->GetTextureID( world.mPreview ) : ImTextureID{};
    }
}// namespace mikoto::editor
