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
#include <imgui_internal.h>

#include <EASTL/utility.h>
#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Assets/Utility.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/Utility.hh>
#include <Material/TextMaterial.hh>
#include <Material/VolumeMaterial.hh>
#include <Material/SkyboxMaterial.hh>
#include <Material/ParticleMaterial.hh>
#include <Material/PostProcessMaterial.hh>

#include <Scripting/ScriptingService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsFontAwesome6.h>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/ContentBrowserPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    static constexpr core::f32 kCardRounding{ 5.0f };
    static constexpr core::f32 kCardFooterOpacity{ 0.75f };
    static constexpr core::f32 kCardDividerHeight{ 2.0f };
    static constexpr core::f32 kCardLabelGap{ 4.0f };
    static constexpr core::f32 kCardNameLineCount{ 2.0f };
    static constexpr core::f32 kCardBadgePaddingX{ 6.0f };
    static constexpr core::f32 kCardBadgePaddingY{ 1.0f };
    static constexpr core::f32 kCardBadgeRounding{ 3.0f };
    static constexpr core::f32 kCardCodeGlyphScale{ 0.6f };

    // Match the Hierarchy panel's selected row and text colors.
    static constexpr ImVec4 kCardAccent{ 232.0f / 255.0f, 191.0f / 255.0f, 124.0f / 255.0f, 1.0f };
    static constexpr ImVec4 kCardBadgeText{ 47.0f / 255.0f, 41.0f / 255.0f, 32.0f / 255.0f, 1.0f };

    static auto GetCardHeight( core::f32 width, bool showType ) -> core::f32 {
        const core::f32 lineHeight{ ImGui::GetTextLineHeight() };
        const core::f32 nameHeight{ kCardNameLineCount * lineHeight + 2.0f * kCardLabelGap };
        const core::f32 badgeHeight{ lineHeight + 2.0f * kCardBadgePaddingY + kCardLabelGap };
        return width + kCardDividerHeight + nameHeight + ( showType ? badgeHeight : 0.0f );
    }

    static auto DrawCardText( ImDrawList* drawList, ImVec2 position, core::f32 width,
        core::cstr begin, core::cstr end ) -> void {
        if ( begin == end ) {
            return;
        }

        const ImVec2 textSize{ ImGui::CalcTextSize( begin, end ) };
        const ImVec2 limit{ position.x + width, position.y + ImGui::GetTextLineHeight() };
        position.x += eastl::max( 0.0f, ( width - textSize.x ) * 0.5f );
        ImGui::RenderTextEllipsis( drawList, position, limit, limit.x, begin, end, &textSize );
    }

    static auto DrawCardName( ImDrawList* drawList, ImVec2 position, core::f32 width, eastl::string_view name ) -> void {
        const core::f32 lineHeight{ ImGui::GetTextLineHeight() };
        const auto* end{ name.data() + name.size() };
        const auto* wrap{ ImGui::GetFont()->CalcWordWrapPosition( ImGui::GetFontSize(), name.data(), end, width ) };
        const auto* nextLine{ ImTextCalcWordWrapNextLineStart( wrap, end ) };

        if ( nextLine == end ) {
            position.y += lineHeight * 0.5f;
        }

        DrawCardText( drawList, position, width, name.data(), wrap );
        position.y += lineHeight;
        DrawCardText( drawList, position, width, nextLine, end );
    }

    static auto DrawSettingsSlider( core::cstr id, eastl::string_view label, core::f32& value,
        core::f32 minimum, core::f32 maximum, eastl::string_view format ) -> void {
        constexpr core::f32 sliderWidth{ 240.0f };
        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX };
        const auto& style{ ImGui::GetStyle() };
        const core::cstr labelEnd{ label.data() + label.size() };
        const core::f32 labelWidth{ ImGui::CalcTextSize( label.data(), labelEnd ).x };
        const core::f32 width{ sliderWidth + labelWidth + 2.0f * style.CellPadding.x };

        if ( ImGui::BeginTable( id, 2, flags, ImVec2{ width, 0.0f } ) ) {
            ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthFixed, labelWidth );
            ImGui::TableSetupColumn( "##Slider", ImGuiTableColumnFlags_WidthFixed, sliderWidth );

            ImGui::TableNextColumn();
            ImGui::SetCursorPosY( ImGui::GetCursorPosY() + style.CellPadding.y );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( label.data(), labelEnd );

            ImGui::TableNextColumn();
            imgui::widget::CircleSlider( "##Value", &value, minimum, maximum, format );
            ImGui::EndTable();
        }
    }

    static auto GetFileIcon( filesystem::FileType type ) -> IconType {
        using filesystem::FileType;

        switch ( type ) {
            case FileType::eTtf:
                return IconType::eTrueTypeFont;
            case FileType::eOtf:
            case FileType::eWoff:
                return IconType::eFontFile;
            case FileType::eMp4:
                return IconType::eVideoFile;
            case FileType::eSubtitle:
                return IconType::eSubtitleFile;
            case FileType::eJson:
                return IconType::eJsonFile;
            case FileType::eToml:
            case FileType::eIni:
                return IconType::eConfigFile;
            case FileType::eCpp_Source:
            case FileType::eCpp_Header:
                return IconType::eCppFile;
            case FileType::eJavaScript:
                return IconType::eJavaScriptFile;
            case FileType::ePdf:
                return IconType::ePdfFile;
            case FileType::ePng:
                return IconType::ePngFile;
            case FileType::ePlainText:
                return IconType::eTextFile;
            case FileType::eMikoto_Particle:
                return IconType::eParticles;
            default:
                return IconType::eRegularFile;
        }
    }

    ContentBrowserPanel::ContentBrowserPanel( const ContentBrowserPanelDescription& desc )
        : Panel{ "Explorer", "Panel", desc.mGroup, desc.mIsToggleable },
          mDevice{ desc.mDevice },
          mEditorState{ desc.mState },
          mProjectBasePath{ desc.mProjectBasePath.ToAbsolute() },
          mResourcesBasePath{ desc.mResourcesBasePath },
          mCurrentDirectory{ mProjectBasePath } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_DNS, mPanelName );
        mThumbnailCache = eastl::make_unique<ThumbnailCache>( mEditorState->mThumbnailRenderer, mDevice );
        const auto loadIcon = [&]( IconType type, core::cstr filename ) {
            const filesystem::Path path{ filesystem::PathBuilder{}
                .SetPath( mResourcesBasePath )
                .SetPath( "Icons" )
                .SetPath( filename )
                .Build() };
            const auto result{ mThumbnailCache->CreateThumbnail( path ) };
            mIconTextures[type] = result.mThumbnail;
        };
        loadIcon( IconType::eRegularFile, "icon_regular_file.png" );
        loadIcon( IconType::eFontFile, "icon_text.png" );
        loadIcon( IconType::eParticles, "icon_particles.png" );

        loadIcon( IconType::eFolder, "UxWingIcons/file-manager-icon.png" );
        loadIcon( IconType::eJsonFile, "UxWingIcons/file-json-color-red-icon.png" );
        loadIcon( IconType::ePdfFile, "UxWingIcons/file-pdf-color-green-icon.png" );
        loadIcon( IconType::ePngFile, "UxWingIcons/file-png-color-red-icon.png" );
        loadIcon( IconType::eTextFile, "UxWingIcons/file-txt-color-red-icon.png" );
        loadIcon( IconType::eTrueTypeFont, "UxWingIcons/file-ttf-color-red-icon.png" );
        loadIcon( IconType::eCppFile, "UxWingIcons/file-c-plus-plus-color-red-icon.png" );
        loadIcon( IconType::eJavaScriptFile, "UxWingIcons/file-js-color-red-icon.png" );
        loadIcon( IconType::eConfigFile, "UxWingIcons/settings-video-clip-icon.png" );
        loadIcon( IconType::eSubtitleFile, "UxWingIcons/closed-captions-video-clip-icon.png" );
        loadIcon( IconType::eVideoFile, "UxWingIcons/file-mp4-color-red-icon.png" );
        loadIcon( IconType::eAudioFile, "UxWingIcons/speaker-video-clip-icon.png" );

        mHistory.push_back( mCurrentDirectory );
    }

    auto ContentBrowserPanel::SetProjectDirectory( const filesystem::Path& root, const filesystem::Path& cache ) -> void {
        MKT_BEGIN_PROFILER();
        mProjectBasePath = root.ToAbsolute();
        mCurrentDirectory = mProjectBasePath;
        mSelectedItem = filesystem::Path{};

        // Navigation and pending operations belong to the newly opened project.
        mHistory.clear();
        mHistory.push_back( mCurrentDirectory );
        mHistoryIndex = 0;

        mFileOperation = FileOperation::eNone;
        mOpenOperationDialog = false;
        mOperationError.clear();
        mSearchFilter.Clear();
        mResetScroll = true;

        RefreshDirectories();
        mThumbnailCache->SetDirectory( filesystem::Path{ cache.GetPathTyped<std::filesystem::path>() / "Thumbnails" } );
    }

    auto ContentBrowserPanel::RefreshDirectories() -> void {
        MKT_BEGIN_PROFILER();
        mDirectoryListings.clear();
        mDirectoryRefreshElapsed = 0.0f;
    }

    auto ContentBrowserPanel::NavigateTo( const filesystem::Path& path, bool recordHistory ) -> void {
        MKT_BEGIN_PROFILER();

        if ( path == mCurrentDirectory || !path.IsDirectory() ) {
            return;
        }

        const auto relative{ path.GetPathTyped<std::filesystem::path>().lexically_relative( mProjectBasePath.GetPathTyped<std::filesystem::path>() ) };

        if ( relative.empty() || ( relative.begin() != relative.end() && *relative.begin() == ".." ) ) {
            return;
        }

        if ( recordHistory ) {
            mHistory.resize( mHistoryIndex + 1 );
            mHistory.push_back( path );
            mHistoryIndex = mHistory.size() - 1;
        }

        mCurrentDirectory = path;
        mSelectedItem = filesystem::Path{};
        mResetScroll = true;

        // Do not clear listings here: navigation can happen during recursive tree
        // traversal. Invalidate at the start of the next frame instead.
        mDirectoryRefreshElapsed = 1.0f;
    }

    auto ContentBrowserPanel::GetDirectoryListing( const filesystem::Path& path ) -> const DirectoryListing& {
        MKT_BEGIN_PROFILER();

        if ( const auto found{ mDirectoryListings.find( path ) }; found != mDirectoryListings.end() ) {
            return *found->second;
        }

        auto listing{ eastl::make_unique<DirectoryListing>() };
        std::error_code error{};
        std::filesystem::directory_iterator iterator{ path.GetPathTyped<std::filesystem::path>(), error };
        const std::filesystem::directory_iterator end{};

        while ( !error && iterator != end ) {
            const auto& item{ *iterator };
            std::error_code statusError{};
            const auto status{ item.symlink_status( statusError ) };

            // Avoid following directory links outside the project or into cycles.
            if ( !statusError && !std::filesystem::is_symlink( status ) && item.path().filename() != ".cache" ) {
                const bool directory{ std::filesystem::is_directory( status ) };

                if ( directory || std::filesystem::is_regular_file( status ) ) {
                    filesystem::Path entryPath{ item.path() };
                    const auto type{ filesystem::InferFileTypeFromExtension( entryPath.GetNormalizedExtension() ) };
                    listing->mEntries.push_back( DirectoryEntry{ eastl::move( entryPath ), type, directory } );
                }
            }

            iterator.increment( error );
        }

        if ( error ) {
            listing->mError = error.message().c_str();
        }

        eastl::sort( listing->mEntries.begin(), listing->mEntries.end(),
            []( const DirectoryEntry& left, const DirectoryEntry& right ) {
            if ( left.mIsDirectory != right.mIsDirectory ) {
                return left.mIsDirectory;
            }

            return left.mPath.GetFilename() < right.mPath.GetFilename();
        } );

        const auto entry{ mDirectoryListings.try_emplace( path, eastl::move( listing ) ).first };
        return *entry->second;
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
        MKT_BEGIN_PROFILER();

        if ( ImGui::Button( ICON_MD_SETTINGS "##BrowserSettings" ) ) {
            ImGui::OpenPopup( "BrowserSettings" );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_browser_options" ).c_str() );

        if ( ImGui::BeginPopup( "BrowserSettings" ) ) {
            constexpr core::f32 minThumbnailSize{ 64.0f };
            constexpr core::f32 maxThumbnailSize{ 256.0f };
            DrawSettingsSlider( "##ThumbnailSize", MKT_LOC( "content_browser_thumbnail_size" ),
                mThumbnailSize, minThumbnailSize, maxThumbnailSize, "%.0f px" );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "content_browser_show_file_types", "Show file types" ).c_str(), mShowFileTypeHint );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "content_browser_folders_only_in_sidebar", "Folders only in sidebar" ).c_str(), mShowDirectoryOnlyInSideView );

            ImGui::Separator();

            imgui::CheckBox( imgui::widget::LocalizedLabel( "content_browser_item_shadows", "Item shadows" ).c_str(), mShowItemShadows );
            ImGui::BeginDisabled( !mShowItemShadows );
            DrawSettingsSlider( "##ShadowSize", MKT_LOC( "content_browser_shadow_size" ),
                mItemShadowSize, 0.0f, kThumbnailPadding, "%.1f px" );
            ImGui::EndDisabled();

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth( -1.0f );
        const eastl::string searchText{ MKT_LOC( "content_browser_search_filter" ) };
        const eastl::string formattedText{ string::Format( "{} {}...", ICON_MD_SEARCH, searchText.c_str() ) };

        if ( ImGui::InputTextWithHint( "##ContentBrowserSearch", formattedText.c_str(), mSearchFilter.InputBuf, IM_ARRAYSIZE( mSearchFilter.InputBuf ) ) ) {
            mSearchFilter.Build();
        }

        ImGui::BeginDisabled( mHistoryIndex == 0 );

        if ( ImGui::Button( ICON_MD_CHEVRON_LEFT "##Back" ) && mHistory[mHistoryIndex - 1].IsDirectory() ) {
            --mHistoryIndex;
            NavigateTo( mHistory[mHistoryIndex], false );
        }

        ImGui::EndDisabled();
        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_back" ).c_str() );
        ImGui::SameLine();
        ImGui::BeginDisabled( mHistoryIndex + 1 >= mHistory.size() );

        if ( ImGui::Button( ICON_MD_CHEVRON_RIGHT "##Forward" ) && mHistory[mHistoryIndex + 1].IsDirectory() ) {
            ++mHistoryIndex;
            NavigateTo( mHistory[mHistoryIndex], false );
        }

        ImGui::EndDisabled();
        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_forward" ).c_str() );
        ImGui::SameLine();
        ImGui::BeginDisabled( mCurrentDirectory == mProjectBasePath );

        if ( ImGui::Button( ICON_MD_ARROW_UPWARD "##Parent" ) ) {
            NavigateTo( mCurrentDirectory.GetDirectory() );
        }

        ImGui::EndDisabled();
        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_parent_folder" ).c_str() );

        ImGui::SameLine();
        const eastl::string iconAddItem{ imgui::GetStringFromUnicode( 57669 ) };
        const eastl::string formattedAddIcon{ string::Format( fmt::runtime( MKT_LOC( "content_browser_add_additem" ).c_str() ), iconAddItem ) };

        if ( ImGui::Button( formattedAddIcon.c_str() ) ) {
            ImGui::OpenPopup( "BrowserAddItem" );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_create_a_file_or_folder_here" ).c_str() );
        {
            imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 16.0f, 12.0f } };
            imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 12.0f, 8.0f } };

            if ( ImGui::BeginPopup( "BrowserAddItem" ) ) {
                DrawCreateMenuItems();
                ImGui::EndPopup();
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( string::Format( "{} " ICON_MD_HOME "###Root", MKT_LOC( "content_browser_home" ) ).c_str() ) ) {
            NavigateTo( mProjectBasePath );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_project_assets" ).c_str() );

        ImGui::SameLine();

        if ( ImGui::Button( string::Format( "{} " ICON_MD_REFRESH "###Refresh", MKT_LOC( "content_browser_refresh" ) ).c_str() ) ) {
            RefreshDirectories();
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_refresh_files_and_folders" ).c_str() );

        ImGui::SameLine();

        if ( ImGui::Button( string::Format( "{} " ICON_MD_FOLDER_OPEN "###Explorer", MKT_LOC( "content_browser_browse" ) ).c_str() ) ) {
            filesystem::OpenInExplorer( mCurrentDirectory );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_open_current_folder_in_the_file_manager" ).c_str() );

        ImGui::SameLine();

        DrawBreadcrumbs();
    }

    auto ContentBrowserPanel::DrawBreadcrumbs() -> void {
        MKT_BEGIN_PROFILER();
        struct Crumb {
            filesystem::Path mPath{};
            eastl::string mLabel{};
            core::f32 mWidth{};
        };

        const auto& style{ ImGui::GetStyle() };
        const ImVec2 menuPadding{ style.WindowPadding };
        const ImVec2 menuSpacing{ style.ItemSpacing };
        const core::f32 padding{ style.FramePadding.x + 3.0f };
        const core::f32 height{ ImGui::GetFrameHeight() };
        const core::f32 arrowWidth{ height };

        // Material icon U+E315 (decimal 58133), matching the requested separator.
        const core::f32 separatorWidth{ ImGui::CalcTextSize( ICON_MD_KEYBOARD_ARROW_RIGHT ).x + 4.0f };
        eastl::fixed_vector<Crumb, 20> crumbs{};
        const auto append{ [&]( const filesystem::Path& path, eastl::string_view label ) {
            crumbs.push_back( { path, eastl::string{ label }, 0.0f } );
            crumbs.back().mWidth = ImGui::CalcTextSize( crumbs.back().mLabel.c_str(), nullptr, false ).x + padding * 2.0f;
        } };

        // Breadcrumbs display filesystem names unchanged in every language.
        const auto rootName{ mProjectBasePath.GetFilename() };
        append( mProjectBasePath, rootName.empty() ? mProjectBasePath.GetPath() : rootName );
        const auto relative{ mCurrentDirectory.GetPathTyped<std::filesystem::path>()
            .lexically_relative( mProjectBasePath.GetPathTyped<std::filesystem::path>() ) };
        auto path{ mProjectBasePath.GetPathTyped<std::filesystem::path>() };

        for ( const auto& component: relative ) {
            if ( component == "." || component.empty() ) {
                continue;
            }

            path /= component;
            const filesystem::Path segment{ component };
            append( filesystem::Path{ path }, segment.GetPath() );
        }

        // Give the path its own row when the navigation tools leave too little space.
        if ( ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 12.0f ) {
            ImGui::NewLine();
        }

        const core::f32 width{ eastl::max( 1.0f, ImGui::GetContentRegionAvail().x ) };
        filesystem::Path destination{};
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
        ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, style.FrameRounding );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 } );
        ImGui::PushStyleColor( ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg] );
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0, 0, 0, 0 } );
        const bool visible{ ImGui::BeginChild( "BrowserBreadcrumbs", { width, height }, ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) };

        if ( visible ) {
            const core::f32 contentWidth{ eastl::max( 1.0f, width - arrowWidth ) };
            core::f32 total{ -separatorWidth };

            for ( const auto& crumb: crumbs ) {
                total += crumb.mWidth + separatorWidth;
            }

            core::usize first{ 1 };
            const core::f32 overflowWidth{ height };
            const core::f32 rootBudget{ crumbs[0].mWidth + separatorWidth + height * 2.0f +
                                        ( crumbs.size() > 2 ? overflowWidth + separatorWidth : 0.0f ) };
            const bool showRoot{ crumbs.size() == 1 || contentWidth >= rootBudget };

            if ( total > contentWidth && crumbs.size() > 2 ) {
                first = crumbs.size() - 1;
                core::f32 used{ crumbs[0].mWidth + separatorWidth * 2.0f + overflowWidth + crumbs.back().mWidth };

                while ( first > 1 && used + crumbs[first - 1].mWidth + separatorWidth <= contentWidth ) {
                    used += crumbs[--first].mWidth + separatorWidth;
                }
            }

            const auto choice{ [&]( core::usize index ) {
                const auto& crumb{ crumbs[index] };
                ImGui::PushID( static_cast<int>( index ) );
                const ImVec2 position{ ImGui::GetCursorScreenPos() };

                if ( ImGui::Selectable( "##Location", crumb.mPath == mCurrentDirectory, 0,
                         { crumb.mWidth, ImGui::GetTextLineHeight() } ) ) {
                    destination = crumb.mPath;
                }

                // Draw labels literally: names containing ##, dots or UTF-8 are not IDs.
                ImGui::GetWindowDrawList()->AddText( position, ImGui::GetColorU32( ImGuiCol_Text ), crumb.mLabel.c_str() );
                mikoto::imgui::widget::ItemTooltip( "%s", crumb.mPath.GetC_Str() );
                ImGui::PopID();
            } };

            const auto segment{ [&]( core::usize index ) {
                const auto& crumb{ crumbs[index] };
                const core::f32 available{ contentWidth - ImGui::GetCursorPosX() };

                if ( available <= 0.0f ) {
                    return;
                }

                ImGui::PushID( static_cast<int>( index ) );
                const ImVec2 position{ ImGui::GetCursorScreenPos() };
                const core::f32 buttonWidth{ eastl::min( crumb.mWidth, available ) };

                if ( ImGui::Button( "##Crumb", { buttonWidth, height } ) ) {
                    destination = crumb.mPath;
                }

                mikoto::imgui::widget::ItemTooltip( "%s", crumb.mPath.GetC_Str() );

                auto* draw{ ImGui::GetWindowDrawList() };
                const bool clipped{ buttonWidth < crumb.mWidth };
                const core::f32 suffix{ clipped ? ImGui::CalcTextSize( "..." ).x : 0.0f };
                const core::f32 right{ eastl::max( position.x, position.x + buttonWidth - padding ) };
                const core::f32 y{ position.y + ( height - ImGui::GetTextLineHeight() ) * 0.5f };

                draw->PushClipRect( position, { eastl::max( position.x, right - suffix ), position.y + height }, true );
                draw->AddText( { position.x + padding, y }, ImGui::GetColorU32( ImGuiCol_Text ), crumb.mLabel.c_str() );
                draw->PopClipRect();

                if ( clipped && buttonWidth > suffix + padding ) {
                    draw->AddText( { right - suffix, y }, ImGui::GetColorU32( ImGuiCol_Text ), "..." );
                }

                ImGui::PopID();
                ImGui::SameLine();
            } };

            const auto separator{ [&]() {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled( "%s", ICON_MD_KEYBOARD_ARROW_RIGHT );
                ImGui::SameLine( 0.0f, 4.0f );
            } };

            if ( showRoot ) {
                segment( 0 );

                if ( crumbs.size() > 1 ) {
                    separator();
                }

                if ( first > 1 ) {
                    if ( ImGui::Button( "...##Ancestors", { overflowWidth, height } ) ) {
                        ImGui::OpenPopup( "HiddenFolders" );
                    }

                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_parent_folders" ).c_str() );
                    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, menuPadding );
                    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, menuSpacing );

                    if ( ImGui::BeginPopup( "HiddenFolders" ) ) {
                        for ( core::usize index{ 1 }; index < first; ++index ) {
                            choice( index );
                        }

                        ImGui::EndPopup();
                    }

                    ImGui::PopStyleVar( 2 );
                    ImGui::SameLine();
                    separator();
                }
            }
            else {
                first = crumbs.size() - 1;
            }

            for ( core::usize index{ first }; index < crumbs.size(); ++index ) {
                segment( index );

                if ( index + 1 < crumbs.size() ) {
                    separator();
                }
            }

            ImGui::SetCursorPos( { eastl::max( 0.0f, width - arrowWidth ), 0.0f } );

            if ( imgui::widget::IconButton( "PathMenu", ICON_MD_EXPAND_MORE, { arrowWidth, height } ) ) {
                ImGui::OpenPopup( "PathLocations" );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "content_browser_path_locations" ).c_str() );
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, menuPadding );
            ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, menuSpacing );

            if ( ImGui::BeginPopup( "PathLocations" ) ) {
                for ( core::usize index{}; index < crumbs.size(); ++index ) {
                    choice( index );
                }

                ImGui::Separator();

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_copy_full_path", "Copy full path" ).c_str() ) ) {
                    ImGui::SetClipboardText( mCurrentDirectory.GetC_Str() );
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleVar( 2 );
        }

        ImGui::EndChild();
        ImGui::PopStyleColor( 2 );
        ImGui::PopStyleVar( 3 );

        // Keep breadcrumb paths stable while their buttons/popups are being drawn.
        if ( !destination.IsEmpty() ) {
            NavigateTo( destination );
        }
    }

    auto ContentBrowserPanel::DrawSideHierarchy( const filesystem::Path& root ) -> void {
        MKT_BEGIN_PROFILER();
        const auto& listing{ GetDirectoryListing( root ) };

        for ( const auto& entry: listing.mEntries ) {
            if ( !entry.mIsDirectory && mShowDirectoryOnlyInSideView ) {
                continue;
            }

            ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding };

            if ( !entry.mIsDirectory ) {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }

            if ( entry.mPath == mCurrentDirectory || entry.mPath == mSelectedItem ) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::PushID( entry.mPath.GetC_Str() );

            const bool open{ ImGui::TreeNodeEx( "##node", flags ) };
            const bool nodeClicked{ ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() };

            ImGui::SameLine( 0.0f, 4.0f );

            if ( entry.mIsDirectory ) {
                ImGui::TextColored(
                    open
                        ? ImVec4{ 1.0f, 0.72f, 0.20f, 1.0f }
                        : ImVec4{ 0.85f, 0.60f, 0.15f, 1.0f },
                    "%s",
                    open ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER );
            }
            else {
                ImGui::TextColored(
                    ImVec4{ 0.65f, 0.70f, 0.78f, 1.0f },
                    "%s",
                    ICON_MD_INSERT_DRIVE_FILE );
            }

            ImGui::SameLine( 0.0f, 6.0f );
            ImGui::TextUnformatted( entry.mPath.GetFilename().data() );

            if ( nodeClicked ) {
                if ( entry.mIsDirectory ) {
                    NavigateTo( entry.mPath );
                }
                else {
                    mSelectedItem = entry.mPath;
                }
            }

            if ( entry.mIsDirectory && open ) {
                DrawSideHierarchy( entry.mPath );
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if ( !listing.mError.empty() ) {
            ImGui::TextDisabled( MKT_LOC( "content_browser_cannot_read_folder" ).c_str() );
            mikoto::imgui::widget::ItemTooltip( "%s", listing.mError.c_str() );
        }
    }

    auto ContentBrowserPanel::DrawSidebarShadow() -> void {
        MKT_BEGIN_PROFILER();
        auto* drawList{ ImGui::GetWindowDrawList() };
        const auto minimum{ drawList->GetClipRectMin() };
        const auto maximum{ drawList->GetClipRectMax() };
        const core::f32 width{ eastl::min( 12.0f, maximum.x - minimum.x ) };
        const core::f32 height{ maximum.y - minimum.y };

        if ( width <= 0.0f || height <= 0.0f ) {
            return;
        }

        // Cast leftwards into the sidebar, with the darkest edge at the divider.
        // A short smoothstep fade at each end removes the hard top/bottom cutoff.
        // Subdividing those caps approximates the curved falloff with ImGui quads.
        const core::f32 left{ maximum.x - width };
        const core::f32 knee{ maximum.x - width * 0.35f };
        const core::f32 capHeight{ eastl::min( 24.0f, height * 0.5f ) };
        const auto color = []( core::f32 opacity ) {
            return ImGui::GetColorU32( ImVec4{ 0.0f, 0.0f, 0.0f, opacity } );
        };

        const auto drawBand = [&]( core::f32 top, core::f32 bottom, core::f32 topFade, core::f32 bottomFade ) {
            if ( bottom <= top ) {
                return;
            }

            const ImU32 clear{ color( 0.0f ) };
            const ImU32 topMiddle{ color( 0.04f * topFade ) };
            const ImU32 bottomMiddle{ color( 0.04f * bottomFade ) };
            drawList->AddRectFilledMultiColor( ImVec2{ left, top }, ImVec2{ knee, bottom }, clear, topMiddle, bottomMiddle, clear );
            drawList->AddRectFilledMultiColor( ImVec2{ knee, top }, ImVec2{ maximum.x, bottom }, topMiddle, color( 0.18f * topFade ), color( 0.18f * bottomFade ), bottomMiddle );
        };

        constexpr core::u32 capSegments{ 4 };

        for ( core::u32 segment{}; segment < capSegments; ++segment ) {
            const core::f32 start{ core::as<core::f32>( segment ) / core::as<core::f32>( capSegments ) };
            const core::f32 end{ core::as<core::f32>( segment + 1 ) / core::as<core::f32>( capSegments ) };
            const core::f32 startFade{ start * start * ( 3.0f - 2.0f * start ) };
            const core::f32 endFade{ end * end * ( 3.0f - 2.0f * end ) };
            drawBand( minimum.y + start * capHeight, minimum.y + end * capHeight, startFade, endFade );
            drawBand( maximum.y - end * capHeight, maximum.y - start * capHeight, endFade, startFade );
        }

        drawBand( minimum.y + capHeight, maximum.y - capHeight, 1.0f, 1.0f );
    }

    auto ContentBrowserPanel::OnRender( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mPanelIsVisible ) {
            return;
        }

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        mThumbnailCache->UploadReady();
        mDirectoryRefreshElapsed += eastl::max( 0.0f, timeStep );

        if ( mDirectoryRefreshElapsed >= 1.0f ) {
            RefreshDirectories();

            if ( !mCurrentDirectory.IsDirectory() ) {
                NavigateTo( mProjectBasePath );
            }
        }

        DrawHeader();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const core::f32 footerHeight{ ImGui::GetFrameHeightWithSpacing() };
        const ImVec2 available{ ImGui::GetContentRegionAvail().x, eastl::max( 1.0f, ImGui::GetContentRegionAvail().y - footerHeight ) };

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable |
                                              ImGuiTableFlags_SizingStretchProp |
                                              ImGuiTableFlags_NoBordersInBodyUntilResize };

        // The old table saved stretch sizing, which can suppress the default width
        // of the new fixed sidebar. Give this layout its own saved settings ID.
        if ( ImGui::BeginTable( "ContentBrowserSplitView", 2, tableFlags, available ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "content_browser_folders" ).c_str(), ImGuiTableColumnFlags_WidthFixed, 210.0f );
            ImGui::TableSetupColumn( MKT_LOC( "content_browser_files" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableNextColumn();
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 16.0f, 10.0f } );
            const bool sideViewVisible{ ImGui::BeginChild( "##SideViewChild", ImVec2{ 0.0f, available.y },
                ImGuiChildFlags_AlwaysUseWindowPadding ) };

            ImGui::PopStyleVar();

            if ( sideViewVisible ) {
                DrawSideHierarchy( mProjectBasePath );
                DrawSidebarShadow();
            }

            ImGui::EndChild();
            ImGui::TableNextColumn();
            // Keep a consistent inset around the cards, independent of the split.
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 16.0f, 10.0f } );
            const bool mainViewVisible{ ImGui::BeginChild( "##MainViewChild", ImVec2{ 0.0f, available.y },
                ImGuiChildFlags_AlwaysUseWindowPadding ) };

            ImGui::PopStyleVar();

            if ( mainViewVisible ) {
                if ( mResetScroll ) {
                    ImGui::SetScrollY( 0.0f );
                    mResetScroll = false;
                }

                DrawCurrentDirItems();

                // Check after submitting the cards so their hit areas are known.
                // Only the file canvas clears selection; popups and active controls
                // (including the scrollbar and dragged thumbnails) keep their input.
                if ( ImGui::IsWindowHovered() && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) &&
                     !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive() ) {
                    mSelectedItem = filesystem::Path{};
                }

                DrawBlankSpaceRightClickMenu();
            }

            ImGui::EndChild();
            ImGui::EndTable();
        }

        ImGui::TextDisabled( MKT_LOC( "content_browser_items" ).c_str(), core::as<core::u64>( mFilteredEntries.size() ) );

        if ( !mSelectedItem.IsEmpty() ) {
            ImGui::SameLine();
            ImGui::TextUnformatted( mSelectedItem.GetFilename().data() );
        }

        DrawFileOperationDialog();
        ImGui::End();
    }

    auto ContentBrowserPanel::DrawFolderEntry( const filesystem::Path& path, core::f32 size ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::PushID( path.GetC_Str() );
        const core::f32 width{ size + 2.0f * kThumbnailPadding };
        const auto& texture{ mIconTextures.at( IconType::eFolder ) };
        const ImTextureID icon{ texture ? imgui::ImGuiService::Get()->GetTextureID( texture ) : ImTextureID{} };

        constexpr core::f32 folderRounding{ 2.0f };
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ kThumbnailPadding, kThumbnailPadding } );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, folderRounding );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{} );
        const auto tint{ ImGui::GetStyleColorVec4( ImGuiCol_Text ) };
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4{ tint.x, tint.y, tint.z, 0.035f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4{ tint.x, tint.y, tint.z, 0.06f } );

        if ( icon != ImTextureID{} ) {
            if ( mShowItemShadows && mItemShadowSize > 0.0f ) {
                // Tint the folder icon so the shadow follows its silhouette.
                const auto position{ ImGui::GetCursorScreenPos() };
                const core::f32 inset{ kThumbnailPadding + mItemShadowSize };
                const ImVec2 shadowMin{ position.x + inset, position.y + inset };
                const ImVec2 shadowMax{ shadowMin.x + size, shadowMin.y + size };
                ImGui::GetWindowDrawList()->AddImage( icon, shadowMin, shadowMax, { 0, 0 }, { 1, 1 },
                    ImGui::GetColorU32( ImVec4{ 0, 0, 0, 0.30f } ) );
            }

            imgui::widget::FadingImageButton( "##Thumbnail", icon, ImVec2{ size, size } );
        }
        else {
            ImGui::Button( ICON_MD_FOLDER, ImVec2{ width, width } );
        }

        ImGui::PopStyleColor( 3 );
        ImGui::PopStyleVar( 3 );

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) || ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
            mSelectedItem = path;
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            imgui::widget::ItemTooltip( "%s", path.GetC_Str() );

            if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                NavigateTo( path );
            }
        }

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{} );
        ImGui::PushStyleVar( ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f } );
        imgui::CenteredText( path.GetFilename().data(), width, ImGui::GetTextLineHeight() );
        imgui::widget::ItemTooltip( "%s", path.GetC_Str() );
        ImGui::PopStyleVar( 2 );
        ImGui::PopID();
    }

    auto ContentBrowserPanel::DrawEntry( const DirectoryEntry& entry, core::f32 size ) -> void {
        MKT_BEGIN_PROFILER();

        if ( entry.mIsDirectory ) {
            DrawFolderEntry( entry.mPath, size );
            return;
        }

        ImGui::PushID( entry.mPath.GetC_Str() );
        const bool audio{ asset::IsFileAudio( entry.mPath ) };
        const bool csharp{ entry.mType == filesystem::FileType::eCSharp || entry.mType == filesystem::FileType::eCSharpProject };
        const IconType iconType{ audio ? IconType::eAudioFile : GetFileIcon( entry.mType ) };
        const bool image{ asset::IsFileImage( entry.mType ) };
        const bool model{ asset::IsFileModel( entry.mType ) };
        const bool material{ entry.mType == filesystem::FileType::eMikoto_Material };

        renderer::rhi::TextureHandle iconTexture{ csharp ? renderer::rhi::TextureHandle{} : mIconTextures.at( iconType ) };
        renderer::rhi::TextureHandle thumbnail{};

        // Use the file icon until a generated preview is available.
        if ( image || model || audio || material || entry.mType == filesystem::FileType::eMp4 ||
             entry.mType == filesystem::FileType::eMikoto_Particle ) {
            if ( mThumbnailCache->Contains( entry.mPath ) ) {
                thumbnail = mThumbnailCache->GetThumbnail( entry.mPath ).mThumbnail;
            }
            else {
                mThumbnailCache->CreateThumbnailAsync( entry.mPath );
            }

            if ( thumbnail ) {
                iconTexture = thumbnail;
            }
        }

        // Refresh the descriptor's frame usage even for fixed file icons.
        // Cached IDs can otherwise outlive their Vulkan descriptor allocation.
        const ImTextureID icon{ iconTexture ? imgui::ImGuiService::Get()->GetTextureID( iconTexture ) : ImTextureID{} };

        // Give the preview, name and badge one hit area for selection and drag-and-drop.
        const core::f32 cardWidth{ size + 2.0f * kThumbnailPadding };
        ImGui::InvisibleButton( "##Thumbnail", ImVec2{ cardWidth, GetCardHeight( cardWidth, mShowFileTypeHint ) }, ImGuiButtonFlags_EnableNav );

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) || ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
            mSelectedItem = entry.mPath;
        }

        if ( ImGui::IsItemVisible() ) {
            auto* drawList{ ImGui::GetWindowDrawList() };
            const ImVec2 minimum{ ImGui::GetItemRectMin() };
            const ImVec2 maximum{ ImGui::GetItemRectMax() };
            const ImGuiCol background{ ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg };
            const ImU32 accent{ ImGui::GetColorU32( kCardAccent ) };
            const core::f32 dividerY{ minimum.y + cardWidth };
            const ImVec2 previewEnd{ maximum.x, dividerY };
            drawList->AddRectFilled( minimum, previewEnd, ImGui::GetColorU32( background ), kCardRounding, ImDrawFlags_RoundCornersTop );
            drawList->AddRectFilled( { minimum.x, dividerY }, maximum, ImGui::GetColorU32( background, kCardFooterOpacity ),
                kCardRounding, ImDrawFlags_RoundCornersBottom );

            if ( icon != ImTextureID{} ) {
                const core::f32 inset{ thumbnail ? 0.0f : kThumbnailPadding };
                const ImDrawFlags corners{ thumbnail ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll };
                const ImVec2 previewMin{ minimum.x + inset, minimum.y + inset };
                const ImVec2 previewMax{ maximum.x - inset, dividerY - inset };

                if ( mShowItemShadows && mItemShadowSize > 0.0f ) {
                    // Keep the shadow inside the preview and follow the icon's transparent silhouette.
                    const ImVec2 shadowMin{ previewMin.x + mItemShadowSize, previewMin.y + mItemShadowSize };
                    const ImVec2 shadowMax{ previewMax.x + mItemShadowSize, previewMax.y + mItemShadowSize };
                    drawList->PushClipRect( minimum, previewEnd, true );
                    drawList->AddImageRounded( icon, shadowMin, shadowMax, { 0, 0 }, { 1, 1 },
                        ImGui::GetColorU32( ImVec4{ 0, 0, 0, 0.30f } ), kCardRounding, corners );
                    drawList->PopClipRect();
                }

                drawList->AddImageRounded( icon, previewMin, previewMax, { 0, 0 }, { 1, 1 }, ImGui::GetColorU32( IM_COL32_WHITE ), kCardRounding, corners );
            }
            else {
                auto* font{ ImGui::GetFont() };
                const core::cstr glyph{ csharp ? ICON_MD_CODE : ICON_MD_INSERT_DRIVE_FILE };
                const core::f32 fontSize{ csharp ? cardWidth * kCardCodeGlyphScale : ImGui::GetFontSize() };
                const ImVec2 glyphSize{ font->CalcTextSizeA( fontSize, cardWidth, 0.0f, glyph ) };
                const ImVec2 position{ minimum.x + ( cardWidth - glyphSize.x ) * 0.5f, minimum.y + ( cardWidth - glyphSize.y ) * 0.5f };
                drawList->AddText( font, fontSize, position, ImGui::GetColorU32( ImGuiCol_Text ), glyph );
            }

            drawList->AddRectFilled( { minimum.x, dividerY }, { maximum.x, dividerY + kCardDividerHeight }, accent );

            const core::f32 labelWidth{ cardWidth - 2.0f * kCardLabelGap };
            DrawCardName( drawList, { minimum.x + kCardLabelGap, dividerY + kCardDividerHeight + kCardLabelGap }, labelWidth, entry.mPath.GetFilename() );

            if ( mShowFileTypeHint ) {
                eastl::string_view type{ filesystem::GetFileTypeDisplayName( entry.mType ) };
                if ( entry.mType == filesystem::FileType::eMikoto_Material ) {
                    type = MKT_LOC( "content_browser_material" );
                }

                const core::f32 lineHeight{ ImGui::GetTextLineHeight() };
                const core::f32 badgeHeight{ lineHeight + 2.0f * kCardBadgePaddingY };
                const core::f32 textWidth{ ImGui::CalcTextSize( type.data(), type.data() + type.size() ).x };
                const core::f32 badgeWidth{ eastl::min( labelWidth, textWidth + 2.0f * kCardBadgePaddingX ) };
                const ImVec2 badgeMin{ minimum.x + ( cardWidth - badgeWidth ) * 0.5f, maximum.y - kCardLabelGap - badgeHeight };
                const ImVec2 badgeMax{ badgeMin.x + badgeWidth, badgeMin.y + badgeHeight };
                drawList->AddRectFilled( badgeMin, badgeMax, accent, kCardBadgeRounding );

                ImGui::PushStyleColor( ImGuiCol_Text, kCardBadgeText );
                const core::f32 textInset{ eastl::min( kCardBadgePaddingX, badgeWidth * 0.25f ) };
                DrawCardText( drawList, { badgeMin.x + textInset, badgeMin.y + kCardBadgePaddingY }, badgeWidth - 2.0f * textInset, type.data(), type.data() + type.size() );
                ImGui::PopStyleColor();
            }

            if ( mSelectedItem == entry.mPath ) {
                drawList->AddRect( minimum, maximum, accent, kCardRounding );
            }
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            mikoto::imgui::widget::ItemTooltip( "%s", entry.mPath.GetC_Str() );

            if ( entry.mType == filesystem::FileType::eMikoto_Material && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                try {
                    if ( auto material{ asset::AssetRegistryService::Get()->LoadAsset<asset::Material>( entry.mPath ) } ) {
                        mEditorState->mEditMaterial = eastl::move( material );
                        mEditorState->mRequestMaterialEditor = true;
                    }
                } catch ( const std::exception& error ) {
                    MKT_CORE_LOGGER_ERROR( "Cannot open material '{}': {}", entry.mPath.GetC_Str(), error.what() );
                }
            }
            else if ( audio && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                mEditorState->mPreviewAudio = asset::AssetRef<asset::AudioClip>{ entry.mPath };
                mEditorState->mRequestAudioPanel = true;
            }
            else if ( entry.mType == filesystem::FileType::eMikoto_Particle && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                if ( mEditorState->mOpenParticle ) {
                    mEditorState->mOpenParticle( entry.mPath );
                }
            }
            else if ( entry.mType == filesystem::FileType::eMikoto_Scene && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                if ( mEditorState->mOpenScene ) {
                    mEditorState->mOpenScene( entry.mPath );
                }
            }
            else if ( entry.mType == filesystem::FileType::eMikoto_Project && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                if ( mEditorState->mOpenProject ) {
                    mEditorState->mOpenProject( entry.mPath );
                }
            }
        }

        const bool scene{ entry.mType == filesystem::FileType::eMikoto_Scene };
        const bool project{ entry.mType == filesystem::FileType::eMikoto_Project };
        const bool video{ entry.mType == filesystem::FileType::eMp4 };

        if ( ( image || model || material || scene || project || video ) && ImGui::BeginDragDropSource() ) {
            // ImGui copies bytes, not C++ objects. Payloads own UTF-8 text rather
            // than shallow-copying Path's strings and native filesystem storage.
            const auto path{ entry.mPath.GetPath() };
            const core::cstr payloadType{ image ? "CONTENT_BROWSER_TEXT_IMAGE" : model  ? "CONTENT_BROWSER_TEXT_MODEL"
                                                                             : material ? "CONTENT_BROWSER_TEXT_MATERIAL"
                                                                             : scene    ? "CONTENT_BROWSER_TEXT_SCENE"
                                                                             : video    ? "CONTENT_BROWSER_VIDEO"
                                                                                        : "CONTENT_BROWSER_TEXT_PROJECT" };
            ImGui::SetDragDropPayload( payloadType, path.data(), path.size() + 1 );

            const auto filename{ entry.mPath.GetFilename() };
            const auto* filenameEnd{ filename.data() + filename.size() };

            if ( icon != ImTextureID{} ) {
                constexpr core::f32 previewSize{ 48.0f };
                const core::f32 imageWidth{ previewSize + 2.0f * ImGui::GetStyle().ImageBorderSize };
                const core::f32 textWidth{ ImGui::CalcTextSize( filename.data(), filenameEnd ).x };
                const core::f32 contentWidth{ eastl::max( imageWidth, textWidth ) };
                const core::f32 originX{ ImGui::GetCursorPosX() };

                ImGui::SetCursorPosX( originX + ( contentWidth - imageWidth ) * 0.5f );
                ImGui::Image( icon, ImVec2{ previewSize, previewSize } );

                ImGui::SetCursorPosX( originX + ( contentWidth - textWidth ) * 0.5f );
            }

            ImGui::TextUnformatted( filename.data(), filenameEnd );
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
    }

    auto ContentBrowserPanel::DrawCurrentDirItems() -> void {
        MKT_BEGIN_PROFILER();
        const auto& listing{ GetDirectoryListing( mCurrentDirectory ) };
        mFilteredEntries.clear();

        for ( core::usize index{}; index < listing.mEntries.size(); ++index ) {
            if ( mSearchFilter.PassFilter( listing.mEntries[index].mPath.GetFilename().data() ) ) {
                mFilteredEntries.push_back( index );
            }
        }

        if ( !listing.mError.empty() ) {
            ImGui::TextWrapped( MKT_LOC( "content_browser_cannot_read_this_folder" ).c_str(), listing.mError.c_str() );
        }

        if ( mFilteredEntries.empty() ) {
            ImGui::TextDisabled( mSearchFilter.IsActive() ? MKT_LOC( "content_browser_no_matching_files_or_folders" ).c_str() : MKT_LOC( "content_browser_this_folder_is_empty" ).c_str() );
            return;
        }

        const auto& style{ ImGui::GetStyle() };
        const core::f32 availableWidth{ ImGui::GetContentRegionAvail().x };
        const core::f32 cellWidth{ mThumbnailSize + kThumbnailPadding * 2.0f + style.CellPadding.x * 2.0f + 12.0f };
        const core::i32 columns{ eastl::clamp( core::as<core::i32>( availableWidth / cellWidth ), 1, 64 ) };
        const core::f32 size{ eastl::max( 1.0f, eastl::min( mThumbnailSize, availableWidth / core::as<core::f32>( columns ) - kThumbnailPadding * 2.0f - style.CellPadding.x * 2.0f ) ) };
        // The listing places folders first, and filtering preserves that order.
        const bool foldersOnly{ listing.mEntries[mFilteredEntries.back()].mIsDirectory };
        const core::f32 width{ size + 2.0f * kThumbnailPadding };
        const core::f32 entryHeight{ foldersOnly ? width + ImGui::GetTextLineHeightWithSpacing() : GetCardHeight( width, mShowFileTypeHint ) };
        const core::f32 rowHeight{ entryHeight + style.ItemSpacing.y + 2.0f * style.CellPadding.y };
        const core::i32 rows{ core::as<core::i32>( ( mFilteredEntries.size() + columns - 1 ) / columns ) };

        if ( ImGui::BeginTable( "ContentBrowserCurrentDir", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings ) ) {
            // Only visible rows request thumbnails or emit widgets. Directory
            // discovery and sorting are cached separately from per-frame filtering.
            ImGuiListClipper clipper{};
            clipper.Begin( rows, rowHeight );

            while ( clipper.Step() ) {
                for ( core::i32 row{ clipper.DisplayStart }; row < clipper.DisplayEnd; ++row ) {
                    ImGui::TableNextRow( ImGuiTableRowFlags_None, rowHeight );

                    for ( core::i32 column{}; column < columns; ++column ) {
                        const core::usize index{ core::as<core::usize>( row ) * columns + column };

                        if ( index >= mFilteredEntries.size() ) {
                            break;
                        }

                        ImGui::TableSetColumnIndex( column );
                        DrawEntry( listing.mEntries[mFilteredEntries[index]], size );
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    auto ContentBrowserPanel::GetNewFileExtension() const -> cstr {
        switch ( mNewFileType ) {
            case NewFileType::eText:
                return ".txt";
            case NewFileType::eLua:
                return ".lua";
            case NewFileType::eCSharp:
                return ".cs";
            case NewFileType::eNative:
                return ".cpp";
            default:
                return ".mktmtl";
        }
    }

    auto ContentBrowserPanel::DrawCreateMenuItems() -> void {
        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_new_folder", "New folder" ).c_str() ) ) {
            BeginFileOperation( FileOperation::eNewFolder );
        }

        ImGui::Separator();
        const auto fileItem{ [&]( core::cstr label, NewFileType type ) {
            if ( ImGui::MenuItem( label ) ) {
                mNewFileType = type;
                BeginFileOperation( FileOperation::eNewFile );
            }
        } };

        if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "content_browser_material", "Material" ).c_str() ) ) {
            fileItem( MKT_LOC( "content_browser_surface_material" ).c_str(), NewFileType::eSurfaceMaterial );
            fileItem( MKT_LOC( "content_browser_particle_material" ).c_str(), NewFileType::eParticleMaterial );
            fileItem( MKT_LOC( "volume_material" ).c_str(), NewFileType::eVolumeMaterial );
            fileItem( MKT_LOC( "content_browser_text_material" ).c_str(), NewFileType::eTextMaterial );
            fileItem( MKT_LOC( "content_browser_skybox_material" ).c_str(), NewFileType::eSkyboxMaterial );
            fileItem( MKT_LOC( "content_browser_color_grading_material" ).c_str(), NewFileType::ePostProcessMaterial );
            ImGui::EndMenu();
        }

        if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "content_browser_script", "Script" ).c_str() ) ) {
            fileItem( "Native (C++)", NewFileType::eNative );
            fileItem( MKT_LOC( "content_browser_c_script" ).c_str(), NewFileType::eCSharp );
            fileItem( MKT_LOC( "content_browser_lua_script" ).c_str(), NewFileType::eLua );
            ImGui::EndMenu();
        }

        fileItem( MKT_LOC( "content_browser_text_file" ).c_str(), NewFileType::eText );
    }

    auto ContentBrowserPanel::BeginFileOperation( FileOperation operation ) -> void {
        MKT_BEGIN_PROFILER();
        mFileOperation = operation;
        const bool create{ operation == FileOperation::eNewFolder || operation == FileOperation::eNewFile };
        mOperationTarget = create ? mCurrentDirectory : mSelectedItem;

        if ( operation == FileOperation::eNewFile ) {
            scripting::ScriptLanguage language{ scripting::ScriptLanguage::eUnknown };

            switch ( mNewFileType ) {
                case NewFileType::eNative:
                    language = scripting::ScriptLanguage::eNative;
                    break;
                case NewFileType::eLua:
                    language = scripting::ScriptLanguage::eLua;
                    break;
                case NewFileType::eCSharp:
                    language = scripting::ScriptLanguage::eCSharp;
                    break;
                default:
                    break;
            }

            if ( language != scripting::ScriptLanguage::eUnknown ) {
                const auto directory{ scripting::ScriptingService::Get()->GetScriptDirectory( language ) };
                const auto relative{ mCurrentDirectory.GetPathTyped<std::filesystem::path>().lexically_relative( directory.GetPathTyped<std::filesystem::path>() ) };
                // Keep subfolders within the language directory; route other locations to it.
                if ( relative.empty() || *relative.begin() == ".." ) {
                    mOperationTarget = directory;
                }
            }
        }

        mNameBuffer.fill( '\0' );

        if ( create ) {
            core::cstr baseName{ "NewFile" };

            switch ( mNewFileType ) {
                case NewFileType::eLua:
                case NewFileType::eNative:
                case NewFileType::eCSharp:
                    baseName = "NewScript";
                    break;
                case NewFileType::eSurfaceMaterial:
                    baseName = "NewMaterial";
                    break;
                case NewFileType::eParticleMaterial:
                    baseName = "NewParticleMaterial";
                    break;
                case NewFileType::eVolumeMaterial:
                    baseName = "NewVolumeMaterial";
                    break;
                case NewFileType::eTextMaterial:
                    baseName = "NewTextMaterial";
                    break;
                case NewFileType::eSkyboxMaterial:
                    baseName = "NewSkyboxMaterial";
                    break;
                case NewFileType::ePostProcessMaterial:
                    baseName = "NewColorGradingMaterial";
                    break;
                default:
                    break;
            }

            const eastl::string name{ operation == FileOperation::eNewFolder ? "NewFolder" : string::Format( "{}{}", baseName, GetNewFileExtension() ) };
            eastl::copy_n( name.data(), eastl::min( name.size(), mNameBuffer.size() - 1 ), mNameBuffer.data() );
        }

        if ( operation == FileOperation::eRename ) {
            const auto filename{ mOperationTarget.GetFilename() };
            const core::usize length{ eastl::min( filename.size(), mNameBuffer.size() - 1 ) };
            eastl::copy_n( filename.data(), length, mNameBuffer.data() );
        }

        mOperationError.clear();
        mOpenOperationDialog = true;
    }

    auto ContentBrowserPanel::DrawBlankSpaceRightClickMenu() -> void {
        MKT_BEGIN_PROFILER();
        // Give the popup room around its edges and taller, easier-to-hit menu rows.
        imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 16.0f, 12.0f } };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 12.0f, 8.0f } };

        // Open once on release for both cards and empty space. Mixing a manual
        // press-triggered open with BeginPopupContextWindow's release trigger
        // could reopen the popup and restart its sizing/positioning for a frame.
        if ( ImGui::IsMouseReleased( ImGuiMouseButton_Right ) && ImGui::IsWindowHovered() ) {
            ImGui::OpenPopup( "BrowserContextMenu" );
        }

        if ( ImGui::BeginPopup( "BrowserContextMenu" ) ) {
            DrawCreateMenuItems();
            ImGui::Separator();

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_refresh", "Refresh" ).c_str() ) ) {
                mDirectoryRefreshElapsed = 1.0f;
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_open_folder_in_explorer", "Open folder in Explorer" ).c_str() ) ) {
                filesystem::OpenInExplorer( mCurrentDirectory );
            }

            if ( !mSelectedItem.IsEmpty() ) {
                ImGui::Separator();

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_copy_path", "Copy path" ).c_str() ) ) {
                    ImGui::SetClipboardText( mSelectedItem.GetC_Str() );
                }

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_show_in_explorer", "Show in Explorer" ).c_str() ) ) {
                    filesystem::OpenInExplorer( mSelectedItem );
                }

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_rename", "Rename" ).c_str() ) ) {
                    BeginFileOperation( FileOperation::eRename );
                }

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "content_browser_remove", "Remove" ).c_str() ) ) {
                    BeginFileOperation( FileOperation::eRemove );
                }
            }

            ImGui::EndPopup();
        }
    }

    auto ContentBrowserPanel::ApplyFileOperation() -> void {
        MKT_BEGIN_PROFILER();

        if ( mFileOperation == FileOperation::eNone || mOperationTarget.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_no_file_operation_is_pending" ).c_str() );
        }

        const auto relative{ mOperationTarget.GetPathTyped<std::filesystem::path>().lexically_relative( mProjectBasePath.GetPathTyped<std::filesystem::path>() ) };
        const bool create{ mFileOperation == FileOperation::eNewFolder || mFileOperation == FileOperation::eNewFile };

        if ( relative.empty() || *relative.begin() == ".." || ( relative == "." && !create ) ) {
            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_choose_an_item_inside_the_asset_folder" ).c_str() );
        }

        const bool remove{ mFileOperation == FileOperation::eRemove };
        const auto source{ mOperationTarget.GetPathTyped<std::filesystem::path>() };

        if ( remove ) {
            std::filesystem::remove( source );
            mSelectedItem = filesystem::Path{};
        }
        else {
            const filesystem::Path name{ mNameBuffer.data() };
            const auto nativeName{ name.GetPathTyped<std::filesystem::path>() };
            // Accept one filename only: dialog input cannot navigate out
            // of its folder, replace existing items, or target the cache.
            if ( name.IsEmpty() || nativeName.has_root_path() || nativeName.has_parent_path() || nativeName == "." || nativeName == ".." || nativeName == ".cache" || name.Contains( ":" ) || name.EndsWith( "." ) || name.EndsWith( " " ) ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_enter_a_single_file_or_folder_name" ).c_str() );
            }

            auto target{ ( create ? source : source.parent_path() ) / nativeName };

            if ( mFileOperation == FileOperation::eNewFile ) {
                const core::cstr extension{ GetNewFileExtension() };

                if ( !nativeName.has_extension() ) {
                    target += extension;
                }
                else if ( name.GetNormalizedExtension() != extension + 1 ) {
                    MKT_THROW_RUNTIME_ERROR( string::Format( fmt::runtime( MKT_LOC( "content_browser_use_the_extension_for_this_file_type" ).c_str() ), extension ) );
                }
            }

            if ( std::filesystem::exists( target ) ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_an_item_with_that_name_already_exists" ).c_str() );
            }

            if ( mFileOperation == FileOperation::eNewFolder ) {
                if ( !std::filesystem::create_directory( target ) ) {
                    MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_the_folder_could_not_be_created" ).c_str() );
                }
            }
            else if ( mFileOperation == FileOperation::eNewFile ) {
                std::filesystem::create_directories( source );
                const filesystem::Path assetPath{ target };

                switch ( mNewFileType ) {
                    case NewFileType::eSurfaceMaterial: {
                        material::SurfaceMaterial asset{ assetPath.GetStem() };
                        asset.SetMetallicFactor( 0.0f );
                        asset.SetRoughnessFactor( 0.5f );
                        asset.Serialize( assetPath );
                        break;
                    }

                    case NewFileType::eParticleMaterial: {
                        material::ParticleMaterial{ assetPath.GetStem() }.Serialize( assetPath );
                        break;
                    }
                    case NewFileType::eVolumeMaterial: {
                        material::VolumeMaterial{ assetPath.GetStem() }.Serialize( assetPath );
                        break;
                    }

                    case NewFileType::eTextMaterial: {
                        material::TextMaterial{ assetPath.GetStem() }.Serialize( assetPath );
                        break;
                    }

                    case NewFileType::eSkyboxMaterial: {
                        material::SkyboxMaterial{ assetPath.GetStem() }.Serialize( assetPath );
                        break;
                    }

                    case NewFileType::ePostProcessMaterial: {
                        material::PostProcessMaterial{ assetPath.GetStem() }.Serialize( assetPath );
                        break;
                    }

                    case NewFileType::eNative: {
                        if ( !scripting::ScriptingService::Get()->CreateScript( scripting::ScriptLanguage::eNative, assetPath ) ) {
                            MKT_THROW_RUNTIME_ERROR( "Could not create the native script. Check the console for details." );
                        }

                        break;
                    }

                    case NewFileType::eCSharp: {
                        if ( !scripting::ScriptingService::Get()->CreateScript( scripting::ScriptLanguage::eCSharp, assetPath ) ) {
                            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_could_not_create_the_c_script_use_letters_digits_and" ).c_str() );
                        }

                        break;
                    }

                    case NewFileType::eLua: {
                        if ( !scripting::ScriptingService::Get()->CreateScript( scripting::ScriptLanguage::eLua, assetPath ) ) {
                            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "content_browser_could_not_create_the_lua_script_check_the_console_for" ).c_str() );
                        }

                        break;
                    }

                    case NewFileType::eText: {
                        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( assetPath ) };

                        if ( file.IsEmpty() ) {
                            MKT_THROW_RUNTIME_ERROR( "Cannot create text file." );
                        }

                        file->Write( "" );
                        break;
                    }
                }
            }
            else {
                std::filesystem::rename( source, target );
            }

            if ( create ) {
                NavigateTo( filesystem::Path{ target.parent_path() } );
                mSearchFilter.Clear();
                mResetScroll = true;
            }

            mSelectedItem = filesystem::Path{ target };
        }

        RefreshDirectories();
        mFileOperation = FileOperation::eNone;
    }

    auto ContentBrowserPanel::DrawFileOperationDialog() -> void {
        MKT_BEGIN_PROFILER();

        if ( mOpenOperationDialog ) {
            ImGui::OpenPopup( imgui::widget::LocalizedLabel( "content_browser_file_operation", "File operation" ).c_str() );
            mOpenOperationDialog = false;
        }

        if ( ImGui::BeginPopupModal( imgui::widget::LocalizedLabel( "content_browser_file_operation", "File operation" ).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
            if ( mFileOperation == FileOperation::eNone ) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            const bool remove{ mFileOperation == FileOperation::eRemove };
            ImGui::TextUnformatted( remove ? MKT_LOC( "content_browser_remove_selected_item" ).c_str() : ( mFileOperation == FileOperation::eRename ? MKT_LOC( "content_browser_rename_item" ).c_str() : ( mFileOperation == FileOperation::eNewFile ? MKT_LOC( "content_browser_create_file" ).c_str() : MKT_LOC( "content_browser_create_folder" ).c_str() ) ) );
            ImGui::TextUnformatted( mOperationTarget.GetC_Str() );

            if ( mFileOperation == FileOperation::eNewFile ) {
                ImGui::TextDisabled( MKT_LOC( "content_browser_file_type_extension_added_if_omitted" ).c_str(), GetNewFileExtension() );
            }

            bool submit{};

            if ( remove ) {
                ImGui::TextDisabled( MKT_LOC( "content_browser_only_files_and_empty_folders_can_be_removed" ).c_str() );
            }
            else {
                if ( ImGui::IsWindowAppearing() ) {
                    ImGui::SetKeyboardFocusHere();
                }

                ImGui::SetNextItemWidth( 320.0f );
                submit = ImGui::InputText( imgui::widget::LocalizedLabel( "content_browser_name", "Name" ).c_str(), mNameBuffer.data(), mNameBuffer.size(),
                    ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue );
            }

            if ( !mOperationError.empty() ) {
                ImGui::TextWrapped( "%s", mOperationError.c_str() );
            }

            const bool create{ mFileOperation == FileOperation::eNewFolder || mFileOperation == FileOperation::eNewFile };

            if ( ImGui::Button( remove ? MKT_LOC( "content_browser_remove" ).c_str() : ( create ? MKT_LOC( "content_browser_create" ).c_str() : MKT_LOC( "content_browser_apply" ).c_str() ) ) || submit ) {
                try {
                    ApplyFileOperation();
                    ImGui::CloseCurrentPopup();
                } catch ( const std::exception& error ) {
                    mOperationError = error.what();
                }
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "content_browser_cancel", "Cancel" ).c_str() ) ) {
                mFileOperation = FileOperation::eNone;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}// namespace mikoto::editor
