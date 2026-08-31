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

#include <filesystem>

#include <EASTL/deque.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/fixed_string.h>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

#include <Memory/Allocator.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Layers/EditorLayer.hh>
#include <Panels/ContentBrowserPanel.hh>

namespace mikoto::editor {

    using namespace mikoto::gui;
    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    ContentBrowserPanel::ContentBrowserPanel( const ContentBrowserPanelDescription& desc )
        : Panel{ "Explorer" },
          mDevice{ desc.mDevice },
          mEditorState{ desc.mState },
          mProjectBasePath{ desc.mProjectBasePath },
          mResourcesBasePath{ desc.mResourcesBasePath },
          mCurrentDirectory{ desc.mProjectBasePath } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_DNS, mPanelName );

        mThumbnailCache = eastl::make_unique<ThumbnailCache>( mDevice );
        // Load files icon
        Path file{ PathBuilder()
             .SetPath( mResourcesBasePath )
             .SetPath( "Icons" )
             .SetPath( "file4.png" )
             .Build() };

        auto result{ mThumbnailCache->CreateThumbnail( file ) };
        mThumbnailHandles[IconType::eRegularFile] = ImGuiService::Get()->GetTextureID( result.mThumbnail );

        // Load folder icon
        Path folder{ PathBuilder()
            .SetPath( mResourcesBasePath )
            .SetPath( "Icons" )
            .SetPath( "folder0.png" )
            .Build() };
        result = mThumbnailCache->CreateThumbnail( folder );
        mThumbnailHandles[IconType::eFolder] = ImGuiService::Get()->GetTextureID( result.mThumbnail );
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGuiScopedStyleVar roundedButtons{ ImGuiStyleVar_FrameRounding, 1.5f };

        // Settings for the content browser
        if ( gui::ButtonTextIcon( ICON_MD_SETTINGS_APPLICATIONS ) ) {
            ImGui::OpenPopup( "HeaderSettingsPopup" );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        if ( ImGui::BeginPopup( "HeaderSettingsPopup" ) ) {
            if ( gui::ButtonTextIcon( ICON_MD_RESTORE ) ) {
                mThumbnailSize = 128.0f;
            }

            ImGui::SameLine();
            ImGui::Text( "Browser thumbnail size" );
            (void)Slider( "##HeaderSettingsPopupThumbnailSize", mThumbnailSize, { 90.0f, 256.0f } );

            ImGui::Spacing();
            ImGui::Separator();

            (void)CheckBox( "##ShowFileTypeHint", mShowFileTypeHint );
            ImGui::SameLine();
            ImGui::Text( "Show file type hint in explorer" );

            (void)CheckBox( "##ShowFilesOnlySideView", mShowDirectoryOnlyInSideView );
            ImGui::SameLine();
            ImGui::Text( "Show folders only in side view" );

            ImGui::EndPopup();
        }

        // Search bar/filter
        ImGui::SameLine();

        const float cursorPosX{ ImGui::GetCursorPosX() };
        mSearchFilter.Draw( "###ContentBrowserFilter", ImGui::GetContentRegionAvail().x );
        if ( !mSearchFilter.IsActive() ) {
            ImGui::SameLine();
            ImGui::SetCursorPosX( cursorPosX + ImGui::GetFontSize() * 0.5f );

            // TODO: grab the color from text color and lower alpha value
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));
            ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 255, 255, 128 ) );

            ImGui::TextUnformatted( fmt::format( "{} Search...", ICON_MD_SEARCH ).c_str() );

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.0f };

        // Back button
        {
            bool disabledBackButton{ false };
            if ( mCurrentDirectory == mProjectBasePath )
                disabledBackButton = true;

            if ( disabledBackButton ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( fmt::format( "{}", ICON_MD_CHEVRON_LEFT ).c_str() ) ) {
                mForwardDirectory = mDirectoryStack.front();
                mDirectoryStack.pop_front();

                mCurrentDirectory = mDirectoryStack.front();
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( disabledBackButton ) {
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
        }

        ImGui::SameLine();

        // Forward Button
        {
            bool disabledFrontButton{ mForwardDirectory.IsEmpty() };

            if ( disabledFrontButton ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( fmt::format( "{}", ICON_MD_CHEVRON_RIGHT ).c_str() ) ) {
                // update forward directory
                mDirectoryStack.push_front( mForwardDirectory );
                mCurrentDirectory = mForwardDirectory;

                mForwardDirectory = Path{};
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( disabledFrontButton ) {
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
        }

        ImGui::SameLine();

        // Home directory
        if ( ImGui::Button( fmt::format( "{}", ICON_MD_HOME ).c_str() ) ) {
            mCurrentDirectory = mProjectBasePath;
            mForwardDirectory = Path{};

            mDirectoryStack = {};
            mDirectoryStack.push_front( mProjectBasePath );
        }
        SetCursorHandOnLastItemHovered();

        ToolTip( []() -> void {
            ImGui::TextUnformatted( "Go to home" );
        }, ImGui::IsItemHovered() );

        ImGui::SameLine();

        // Folder icon (current directory)
        if ( ImGui::Button( string::Format( "{}", ICON_MD_FOLDER ).c_str() ) ) {
            OpenInExplorer( mCurrentDirectory );
        }
        SetCursorHandOnLastItemHovered();

        ToolTip( []() -> void {
            ImGui::TextUnformatted( "Open in explorer" );
        }, ImGui::IsItemHovered() );

        ImGuiScopedStyleVar frameBorderSize{ ImGuiStyleVar_FrameBorderSize, 0.0f };
        ImGuiScopedColor button{ ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f } };
        ImGuiScopedColor buttonHovered{ ImGuiCol_ButtonHovered, ImVec4{ 0.16f, 0.16f, 0.16f, 0.5f } };
        ImGuiScopedColor buttonActive{ ImGuiCol_ButtonActive, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f } };

        // Directory buttons
        bool firstSlash{ true };
        bool wantOpenDir{ false };

        auto pathIt{ mDirectoryStack.begin() };
        for ( ; pathIt != mDirectoryStack.end(); ++pathIt ) {
            if (firstSlash) {
                firstSlash = false;
            } else {
                ImGui::SameLine();
                ImGui::Text( "/" );
            }

            ImGui::SameLine();
            if ( ImGui::Button( Path{ pathIt->GetAbsolute() }.GetStem().data() ) ) {
                mCurrentDirectory = *pathIt;
                mForwardDirectory = Path{};
                wantOpenDir = true;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( wantOpenDir )
                break;
        }

        mDirectoryStack.erase( pathIt, mDirectoryStack.end() );
        if ( mDirectoryStack.empty() ) {
            mDirectoryStack.push_back( mProjectBasePath );
        }
    }

    auto ContentBrowserPanel::DrawSideHierarchy(const Path& root) -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_FramePadding |
                                                     ImGuiTreeNodeFlags_SpanFullWidth |
                                                    ImGuiTreeNodeFlags_OpenOnArrow };

        for ( auto& entry: std::filesystem::directory_iterator( root.GetPathTyped<std::filesystem::path>() ) ) {
            if (mShowDirectoryOnlyInSideView && !entry.is_directory()) {
                continue;
            }

            std::string nodeIDString{ entry.path().string() };
            if (entry.is_directory()) {
                ImGuiID nodeID{ ImGui::GetID(nodeIDString.c_str()) };

                ImGuiStorage* storage{ ImGui::GetStateStorage() };
                bool isNodeOpened{ storage->GetBool(nodeID, false) };

                bool isOpen{ ImGui::TreeNodeEx( nodeIDString.c_str(), treeNodeFlags, "%s",
                    string::Format( "{} {}", isNodeOpened ? ICON_MD_FOLDER_OPEN : ICON_MD_FOLDER, entry.path().stem().string() ).c_str() ) };

                SetCursorHandOnLastItemHovered();

                if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) && !ImGui::IsItemToggledOpen() ) {
                    mCurrentDirectory = entry.path();
                }

                if ( isOpen ) {
                    DrawSideHierarchy( entry );
                    ImGui::TreePop();
                }

                SetCursorHandOnLastItemHovered();
            } else {

            }
        }

    }

    auto ContentBrowserPanel::DrawMainBody() -> void {
        DrawCurrentDirItems();
    }

    auto ContentBrowserPanel::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mPanelIsVisible ) {
            return;
        }

        static constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_None };
        static constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable |
                                                     ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedSame };

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), windowFlags );

        DrawHeader();

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Separator();

        const ImVec2 availableRegion{ ImGui::GetContentRegionAvail() };

        if ( ImGui::BeginTable( "ContentBrowserMainViewTable", 2, tableFlags, availableRegion ) ) {
            ImGui::TableNextColumn();
            ImGui::BeginChild( "##SideViewChild", ImVec2{ 0, 0 } );

            DrawSideHierarchy( mResourcesBasePath );

            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild( "##MainViewChild", ImVec2{ 0, 0 } );

            DrawMainBody();
            DrawBlankSpaceRightClickMenu();

            ImGui::EndChild();
            ImGui::EndTable();
        }

        ImGui::End();
    }

    auto ContentBrowserPanel::DrawCurrentDirItems() -> void {
        Path directoryToOpen{ mCurrentDirectory };

        constexpr float padding{ 15.0f };
        const float cellSize{ mThumbnailSize + padding };

        const float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount{ static_cast<int>( panelWidth / cellSize ) };
        if ( columnCount < 1 ) {
            columnCount = 1;
        }

        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY };

        if ( ImGui::BeginTable( "ContentBrowserCurrentDir", columnCount, flags ) ) {
            ImGui::TableNextRow();
            for ( auto& entry: std::filesystem::directory_iterator( mCurrentDirectory.GetC_Str() ) ) {
                std::string stem{ entry.path().stem().string() };
                if (!mSearchFilter.PassFilter( stem.c_str() )) {
                    continue;
                }

                ImGui::TableNextColumn();

                ImTextureID icon{};
                std::string fileType{};

                // For image files we load the preview
                if (entry.path().string().ends_with( ".png" )
                    || entry.path().string().ends_with( ".jpg" )
                    || entry.path().string().ends_with( ".hdr" )) {

                    if (mThumbnailCache->Contains( entry.path() )) {
                        mThumbnail = mThumbnailCache->GetThumbnail( entry.path() ).mThumbnail;
                    } else {
                        mThumbnailCache->CreateThumbnailAsync( entry.path() );
                    }

                } else {
                    mThumbnail = TextureHandle::CreateEmpty();
                }

                if ( entry.is_directory() ) {
                    icon = mThumbnailHandles[IconType::eFolder];
                    fileType = "Folder";
                } else {
                    // find type (texture, material, text file) file now for simplicity
                    icon = mThumbnailHandles[IconType::eRegularFile];
                    fileType = "File";
                }

                const auto buttonColor{ mSelectedItem == entry ? ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) : ImVec4( 0, 0, 0, 0 ) };
                ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0, 0, 0, 0 ) );
                if (mThumbnail.IsEmpty()) {
                    if ( ImGui::ImageButton( entry.path().string().c_str(), icon, ImVec2{ mThumbnailSize, mThumbnailSize }, ImVec2{ 0, 0 }, ImVec2{ 1, 1 }, buttonColor ) ) {
                        // empty
                    }
                } else {
                    static ImTextureID imguiTextID{};
                    imguiTextID = ImGuiService::Get()->GetTextureID( mThumbnail );
                    if ( ImGui::ImageButton( entry.path().string().c_str(), imguiTextID, ImVec2{ mThumbnailSize, mThumbnailSize }, ImVec2{ 0, 0 }, ImVec2{ 1, 1 }, buttonColor ) ) {
                        // empty
                    }

                    if (entry.path().string().ends_with( ".png" )
                    || entry.path().string().ends_with( ".jpg" )) {
                        // DRAG SOURCE must be checked after drawing the item
                        if (ImGui::BeginDragDropSource()) {

                            // Send the texture handle
                            ImGui::SetDragDropPayload("CONTENT_BROWSER_TEXT", std::addressof( mThumbnail ), sizeof(TextureHandle));

                            // Preview
                            constexpr float previewDimensions{ 48.0f };
                            ImGui::Image(imguiTextID, ImVec2(previewDimensions, previewDimensions));
                            gui::CenteredText( fmt::format( "Move Icon" ).c_str(), previewDimensions );

                            ImGui::EndDragDropSource();
                        }
                    }

                    // DRAG for HDR load in Lighting panel
                    if (entry.path().string().ends_with( ".hdr" )) {
                        if (ImGui::BeginDragDropSource()) {
                            static std::string path{};

                            path = entry.path().string();

                            // Send the texture handle
                            // Reminder: Payload type can be at most 32 characters long
                            ImGui::SetDragDropPayload("HDR_LOAD_LIGHT_PANEL", std::addressof( path ), sizeof(path));

                            // Preview
                            constexpr float previewDimensions{ 48.0f };
                            ImGui::Image(ImGuiService::Get()->GetTextureID( mThumbnail ), ImVec2(previewDimensions, previewDimensions), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                            gui::CenteredText( fmt::format( "Skybox" ).c_str(), previewDimensions );

                            if ( !ImGui::IsItemClicked() ) {
                                mSelectedItem = "";
                            }

                            ImGui::EndDragDropSource();
                        }
                    }
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                // Save the directory we want to open
                if ( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                    if ( entry.is_directory() ) {
                        directoryToOpen = entry.path();
                        if ( mDirectoryStack.empty() ) {
                            mDirectoryStack.emplace_back( mProjectBasePath );
                        }

                        mDirectoryStack.emplace_back( entry.path() );
                    }
                }

                if ( ImGui::IsItemHovered() && ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) || ImGui::IsMouseClicked( ImGuiMouseButton_Right ) ) ) {
                    mSelectedItem = entry;
                }

                // File name
                ImGui::PopStyleColor();
                gui::CenteredText( fmt::format( "{}", entry.path().stem().string() ).c_str(), mThumbnailSize );

                // Type of file
                if ( mShowFileTypeHint ) {
                    ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 255, 255, 128 ) );
                    gui::CenteredText( fmt::format( "{}", fileType.c_str() ).c_str(), mThumbnailSize );
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndTable();
        }
        mCurrentDirectory = directoryToOpen;
    }

    auto ContentBrowserPanel::DrawBlankSpaceRightClickMenu() -> void {
        ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f } };
        ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGui::OpenPopupOnItemClick( "ContentBrowserPanel::ExploreRC", ImGuiPopupFlags_MouseButtonRight );
        if ( ImGui::BeginPopupContextWindow( "ContentBrowserPanel::ExploreRC", popupWindowFlags ) ) {

            ImGui::Spacing();
            if ( ImGui::MenuItem( fmt::format( "{} Cut", ICON_MD_CONTENT_CUT ).c_str(), "Ctrl + X" ) ) {}
            if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            ImGui::Spacing();
            if ( ImGui::MenuItem( fmt::format( "{} Copy", ICON_MD_CONTENT_COPY ).c_str(), "Ctrl + C" ) ) {}
            if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            ImGui::Spacing();
            if ( ImGui::MenuItem( fmt::format( "{} Paste", ICON_MD_CONTENT_PASTE ).c_str(), "Ctrl + P" ) ) {}
            if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Spacing();
            if ( ImGui::BeginMenu( "Add new..." ) ) {

                ImGui::Spacing();
                if ( ImGui::MenuItem( "Folder" ) ) {
                    ImGui::OpenPopup( "ContentBrowserPopupAddNewFolder" );
                }

                SetCursorHandOnLastItemHovered();

                if ( ImGui::BeginPopupModal( "ContentBrowserPopupAddNewFolder" ) ) {
                    ImGui::Text( "%s Name:", ICON_FA_SEARCH );
                    static eastl::fixed_string<char, 256> buffer{};

                    if ( ImGui::InputText( "##ContentBrowserPopupAddNewFolderName", buffer.data(), buffer.size() ) ) {
                        //std::filesystem::create_directory( mCurrentDirectory / Path{ buffer.data() } );
                    }

                    if ( ImGui::Button( "Ok" ) ) {
                        ImGui::CloseCurrentPopup();
                    }

                    if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

                    ImGui::EndPopup();
                }

                ImGui::Spacing();
                if ( ImGui::MenuItem( "Material" ) ) {}
                if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

                ImGui::EndMenu();
            }

            if ( !mSelectedItem.IsEmpty() ) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if ( ImGui::MenuItem( fmt::format( "{} Open in explorer", ICON_MD_FOLDER ).c_str() ) ) {
                    filesystem::OpenInExplorer( mSelectedItem );
                }
                SetCursorHandOnLastItemHovered();

                ImGui::Spacing();
                if ( ImGui::MenuItem( "Remove" ) ) {
                    std::filesystem::remove( mSelectedItem.GetC_Str() );
                    mSelectedItem = "";
                }
                SetCursorHandOnLastItemHovered();

                ImGui::Spacing();
                if ( ImGui::MenuItem( "Edit" ) ) {
                    RuntimeConsole::Get()->ExecuteCommand( string::Format( "/code {}", mSelectedItem.GetC_Str() ) );
                }
                SetCursorHandOnLastItemHovered();
            }

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Spacing();
            if ( ImGui::MenuItem( fmt::format( "{} Rename", ICON_MD_DRIVE_FILE_RENAME_OUTLINE ).c_str(), "F5" ) ) {}
            if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            ImGui::Spacing();
            if ( ImGui::MenuItem( fmt::format( "{} Rename", ICON_MD_DELETE_SWEEP ).c_str(), "Delete" ) ) {}
            if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }
} // namespace Mikoto