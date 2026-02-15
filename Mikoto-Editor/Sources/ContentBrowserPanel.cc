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
#include <utility>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <Assets/AssetsService.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Library/String/String.hh>
#include <Panels/ContentBrowserPanel.hh>
#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>

#include <Filesystem/FileSystem.hh>

namespace Mikoto {

    ContentBrowserPanel::ContentBrowserPanel( const ContentBrowserPanelDescription& desc )
        : Panel{ "Project" },
          m_Device{ desc.Device },
          m_ProjectRoot{ desc.ProjectRootDirectory },
          m_AssetsRootDirectory{ desc.AssetsRootDirectory }, m_EditorState{ desc.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_DNS, m_PanelName );

        LoadIcons();

        m_CurrentDirectory = m_ProjectRoot;
        m_ForwardDirectory = Path{};
    }

    auto ContentBrowserPanel::LoadIcons() -> void {
        // Load files icon
        Path file{ PathBuilder()
                             .WithPath( m_AssetsRootDirectory.string() )
                             .WithPath( "Icons" )
                             .WithPath( "file4.png" )
                             .Build() };

        const TextureLoadDescription fileTextureDesc{
            .TextureFile{ FileService::Get()->LoadFile( file ) },
            .Type{ TextureType::TEXTURE_2D },
        };
        TextureHandle fileTexture{ AssetsService::Get()->LoadAsset<Texture>( fileTextureDesc ) };

        // Load folder icon
        Path folder{ PathBuilder()
                               .WithPath( m_AssetsRootDirectory.string() )
                               .WithPath( "Icons" )
                               .WithPath( "folder0.png" )
                               .Build() };
        const TextureLoadDescription folderTextureDesc{
            .TextureFile{ FileService::Get()->LoadFile( folder ) },
            .Type{ TextureType::TEXTURE_2D },
        };
        TextureHandle folderTexture{ AssetsService::Get()->LoadAsset<Texture>( folderTextureDesc ) };

        ImGuiBackend *backend{ ImGuiService::Get()->GetBackend() };

        m_ImGuiTextureHandles.emplace( std::make_pair( TextureIconType::ICON_FILE, backend->ConstructImGuiTextureID( fileTexture ) ) );
        m_ImGuiTextureHandles.emplace( std::make_pair( TextureIconType::ICON_FOLDER, backend->ConstructImGuiTextureID( folderTexture ) ) );
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 1.5f );// Rounded Buttons

        // Settings for the content browser
        if ( ImGuiUtils::ButtonTextIcon( ICON_MD_SETTINGS_APPLICATIONS ) ) {
            ImGui::OpenPopup( "HeaderSettingsPopup" );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        if ( ImGui::BeginPopup( "HeaderSettingsPopup" ) ) {
            if ( ImGuiUtils::ButtonTextIcon( ICON_MD_RESTORE ) ) {
                m_ThumbnailSize = 128.0f;
            }

            ImGui::SameLine();
            ImGui::Text( "Browser thumbnail size" );
            ImGuiUtils::Slider( "##HeaderSettingsPopupThumbnailSize", m_ThumbnailSize, { 90.0f, 256.0f } );

            ImGui::Spacing();
            ImGui::Separator();

            // Show a file hint (small text under file name)
            ImGuiUtils::CheckBox( "##ShowFileTypeHint", m_ShowFileTypeHint );
            ImGui::SameLine();
            ImGui::Text( "Show file type hint in explorer" );

            ImGui::EndPopup();
        }

        // Search bar/filter
        ImGui::SameLine();

        const float cursorPosX{ ImGui::GetCursorPosX() };
        m_SearchFilter.Draw( "###ContentBrowserFilter", ImGui::GetContentRegionAvail().x );
        if ( !m_SearchFilter.IsActive() ) {
            ImGui::SameLine();
            ImGui::SetCursorPosX( cursorPosX + ImGui::GetFontSize() * 0.5f );

            // TODO: grab the color from text color and lower alpha value
            ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 255, 255, 128 ) );
            ImGui::TextUnformatted( fmt::format( "{} Search...", ICON_MD_SEARCH ).c_str() );
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.0f };

        // Back button
        {
            bool disabledBackButton{ false };
            if ( m_CurrentDirectory == m_ProjectRoot )
                disabledBackButton = true;

            if ( disabledBackButton ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( fmt::format( "{}", ICON_MD_CHEVRON_LEFT ).c_str() ) ) {
                m_ForwardDirectory = m_DirectoryStack.front();
                m_DirectoryStack.pop_front();

                m_CurrentDirectory = m_DirectoryStack.front();
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
            bool disabledFrontButton{ m_ForwardDirectory.empty() };

            if ( disabledFrontButton ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( fmt::format( "{}", ICON_MD_CHEVRON_RIGHT ).c_str() ) ) {
                // update forward directory
                m_DirectoryStack.push_front( m_ForwardDirectory );
                m_CurrentDirectory = m_ForwardDirectory;

                m_ForwardDirectory = Path{};
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
            m_CurrentDirectory = m_ProjectRoot;
            m_ForwardDirectory = Path{};

            m_DirectoryStack = {};
            m_DirectoryStack.push_front( m_ProjectRoot );
        }
        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Folder icon (current directory)
        if ( ImGui::Button( fmt::format( "{}", ICON_MD_FOLDER ).c_str() ) ) {
            Filesystem::OpenInExplorer( m_CurrentDirectory );
        }

        ImGuiUtils::SetCursorHandOnLastItemHovered();

        ImGuiUtils::ToolTip( []() -> void {
            ImGui::TextUnformatted( "Open in explorer" );
        }, ImGui::IsItemHovered() );

        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
        ImGui::PushStyleColor( ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 0.16f, 0.16f, 0.16f, 0.5f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, { 0.0f, 0.0f, 0.0f, 0.0f } );

        // Directory buttons
        bool wantOpenDir{ false };

        auto pathIt{ m_DirectoryStack.begin() };
        for ( ; pathIt != m_DirectoryStack.end(); ++pathIt ) {
            ImGui::SameLine();
            if ( ImGui::Button( pathIt->stem().string().c_str() ) ) {
                m_CurrentDirectory = *pathIt;
                m_ForwardDirectory = Path{};
                wantOpenDir = true;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::SameLine();
            ImGui::Text( "/" );

            if ( wantOpenDir )
                break;
        }

        m_DirectoryStack.erase( pathIt, m_DirectoryStack.end() );
        if ( m_DirectoryStack.empty() ) m_DirectoryStack.push_back( m_ProjectRoot );

        ImGui::PopStyleColor( 3 );
        ImGui::PopStyleVar();

        ImGui::PopStyleVar();// Rounded Buttons
    }

    auto ContentBrowserPanel::DrawSideView(const Path& root) -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_FramePadding |
                                                     ImGuiTreeNodeFlags_SpanFullWidth |
                                                    ImGuiTreeNodeFlags_OpenOnArrow };

        for ( auto& entry: std::filesystem::directory_iterator( m_AssetsRootDirectory ) ) {
            if ( entry.is_directory() ) {
                bool isOpen{ ImGui::TreeNodeEx( entry.path().string().c_str(), treeNodeFlags, "%s", 
                    fmt::format( "{} {}", ICON_MD_FOLDER, entry.path().stem().string() ).c_str() ) };

                ImGuiUtils::SetCursorHandOnLastItemHovered();

                if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) &&
                     !ImGui::IsItemToggledOpen() ) {
                    m_CurrentDirectory = entry.path();
                }

                if ( isOpen ) {
                    
                    DrawSideView( entry );

                    ImGui::TreePop();
                }

                ImGuiUtils::SetCursorHandOnLastItemHovered();
            }
        }
    }

    auto ContentBrowserPanel::DrawMainBody() -> void {
        DrawCurrentDirItems();
    }

    auto ContentBrowserPanel::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_PanelIsVisible ) {
            return;
        }

        static constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_None };
        static constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable |
                                                     ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedSame };

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

        OnRightClick();

        DrawHeader();


        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Separator();

        const ImVec2 availableRegion{ ImGui::GetContentRegionAvail() };

        if ( ImGui::BeginTable( "ContentBrowserMainViewTable", 2, tableFlags, availableRegion ) ) {
            ImGui::TableNextColumn();

            ImGui::BeginChild( "##SideViewChild",
                               ImVec2{ 0, 0 },
                               ImGuiChildFlags_Borders );

            DrawSideView( m_AssetsRootDirectory );

            ImGui::EndChild();


            ImGui::TableNextColumn();

            ImGui::BeginChild( "##MainViewChild",
                               ImVec2{ 0, 0 },
                               ImGuiChildFlags_Borders );

            DrawMainBody();

            ImGui::EndChild();

            ImGui::EndTable();
        }

        ImGui::End();
    }

    auto ContentBrowserPanel::DrawProjectDirTree( const Path& root ) const -> void {
        constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_None };
        constexpr ImGuiTreeNodeFlags childNodeFlags{ styleFlags | ImGuiTreeNodeFlags_DefaultOpen };

        if ( ImGui::TreeNodeEx( reinterpret_cast<const void*>( root.string().c_str() ), childNodeFlags, "%s", root.stem().c_str() ) ) {
            for ( const auto& entry: std::filesystem::directory_iterator( root ) ) {
                if ( entry.is_directory() ) {
                    if ( ImGui::TreeNodeEx( reinterpret_cast<const void*>( entry.path().string().c_str() ), childNodeFlags, "%s", entry.path().stem().c_str() ) ) {

                        ImGui::TreePop();
                    }
                }
            }

            ImGui::TreePop();
        }
    }

    auto ContentBrowserPanel::DrawCurrentDirItems() -> void {
        Path directoryToOpen{ m_CurrentDirectory };

        constexpr float padding{ 15.0f };
        const float cellSize{ m_ThumbnailSize + padding };

        const float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount{ static_cast<int>( panelWidth / cellSize ) };
        if ( columnCount < 1 ) {
            columnCount = 1;
        }

        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY };

        if ( ImGui::BeginTable( "ContentBrowserCurrentDir", columnCount, flags ) ) {
            ImGui::TableNextRow();
            for ( auto& entry: std::filesystem::directory_iterator( m_CurrentDirectory ) ) {
                ImGui::TableNextColumn();

                ImTextureID icon{};
                std::string fileType{};

                // TODO: Figure a better way to preview explorer texture, this right now is costly
                if (entry.path().string().ends_with( ".png" )
                    || entry.path().string().ends_with( ".jpg" )
                    || entry.path().string().ends_with( ".hdr" )) {


                    m_Thumbnail = AssetsService::Get()->GetAssetByUri<Texture>( entry.path().string() );

                    if (m_Thumbnail.IsEmpty()) {
                        // Request upload. Avoid request multiple times
                        if (!m_ThumbnailsUploadCache.contains( entry.path().string() )) {
                            AssetsService::Get()->LoadAssetAsync<Texture>( entry.path() );
                            m_ThumbnailsUploadCache.insert( entry.path().string() );
                        }

                        m_Thumbnail = m_Textures[TextureIconType::ICON_FILE];
                    }

                } else {
                    m_Thumbnail = TextureHandle::CreateEmpty();
                }

                if ( entry.is_directory() ) {
                    icon = m_ImGuiTextureHandles[TextureIconType::ICON_FOLDER];
                    fileType = "Folder";
                } else {
                    // find type (texture, material, text file) file now for simplicity
                    icon = m_ImGuiTextureHandles[TextureIconType::ICON_FILE];
                    fileType = "File";
                }

                ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0, 0, 0, 0 ) );
                if (m_Thumbnail.IsEmpty()) {
                    if ( ImGui::ImageButton( entry.path().string().c_str(), icon, ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } ) ) {
                        // empty
                    }
                } else {
                    static ImTextureID imguiTextID{};
                    imguiTextID = ImGuiService::Get()->GetTextureID( m_Thumbnail );

                    if ( ImGui::ImageButton( entry.path().string().c_str(), imguiTextID, ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } ) ) {
                        // empty
                    }

                    if (entry.path().string().ends_with( ".png" )
                    || entry.path().string().ends_with( ".jpg" )) {
                        // DRAG SOURCE must be checked after drawing the item
                        if (ImGui::BeginDragDropSource()) {

                            // Send the texture handle
                            ImGui::SetDragDropPayload("CONTENT_BROWSER_TEXT", std::addressof( m_Thumbnail ), sizeof(TextureHandle));

                            // Preview
                            constexpr float previewDimensions{ 48.0f };
                            ImGui::Image(imguiTextID, ImVec2(previewDimensions, previewDimensions), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                            ImGuiUtils::CenteredText( fmt::format( "Move Icon" ).c_str(), previewDimensions );

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
                            ImGui::Image(ImGuiService::Get()->GetTextureID( m_Thumbnail ), ImVec2(previewDimensions, previewDimensions), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                            ImGuiUtils::CenteredText( fmt::format( "Skybox" ).c_str(), previewDimensions );

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
                        if ( m_DirectoryStack.empty() ) {
                            m_DirectoryStack.emplace_back( m_ProjectRoot );
                        }

                        m_DirectoryStack.emplace_back( entry.path() );
                    }
                }

                if ( ImGui::IsItemHovered() && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
                    m_SelectedItem = entry;
                }

                // File name
                ImGui::PopStyleColor();
                ImGuiUtils::CenteredText( fmt::format( "{}", entry.path().stem().string() ).c_str(), m_ThumbnailSize );

                // Type of file
                if ( m_ShowFileTypeHint ) {
                    ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 255, 255, 128 ) );
                    ImGuiUtils::CenteredText( fmt::format( "{}", fileType.c_str() ).c_str(), m_ThumbnailSize );
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndTable();
        }

        m_CurrentDirectory = directoryToOpen;
    }

    auto ContentBrowserPanel::OnRightClick() const -> void {
        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        if ( ImGui::BeginPopupContextWindow( "##ContentBrowserPanel::ContentBrowserPopup", popupWindowFlags ) ) {

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

                if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }


                if ( ImGui::BeginPopupModal( "ContentBrowserPopupAddNewFolder" ) ) {
                    ImGui::Text( "%s Name:", ICON_FA_SEARCH );
                    static std::array<char, 256> buffer{};

                    if ( ImGui::InputText( "##ContentBrowserPopupAddNewFolderName", buffer.data(), buffer.size() ) ) {
                        std::filesystem::create_directory( m_CurrentDirectory / Path{ buffer.data() } );
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

                ImGui::Spacing();
                if ( ImGui::MenuItem( "Regular file" ) ) {}
                if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

                ImGui::EndMenu();
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
}// namespace Mikoto