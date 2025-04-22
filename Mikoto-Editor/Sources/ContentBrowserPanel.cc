//
// Created by kate on 10/8/23.
//

#include "Panels/ContentBrowserPanel.hh"

#include <GUI/Icons/IconsMaterialDesignIcons.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#include <volk.h>

#include <Assets/AssetsService.hh>
#include <GUI/ImGuiService.hh>
#include <GUI/ImGuiUtility.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/String/String.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <filesystem>
#include <utility>

#include "GUI/Icons/IconsFontAwesome5.h"
#include "GUI/Icons/IconsMaterialDesign.h"

namespace Mikoto {

    MKT_NODISCARD static constexpr auto GetContentBrowserName() -> std::string_view {
        return "Project";
    }

    ContentBrowserPanel::ContentBrowserPanel( const ContentBrowserPanelDescription& desc )
        : Panel{ StringUtils::MakePanelName( ICON_MD_DNS, GetContentBrowserName() ) },
          m_Device{ desc.Device },
          m_ProjectRoot{ desc.ProjectRootDirectory },
          m_AssetsRootDirectory{ desc.AssetsRootDirectory } {
        LoadIcons();

        m_CurrentDirectory = m_ProjectRoot;
        m_ForwardDirectory = Path_T{};
    }

    auto ContentBrowserPanel::LoadIcons() -> void {
        // Load files icon
        Path_T file{ PathBuilder()
                             .WithPath( m_AssetsRootDirectory.string() )
                             .WithPath( "file4.png" )
                             .Build() };

        constexpr TextureLoadDescription fileTextureDesc{
            .Type{ TextureType::TEXTURE_2D },
        };
        TextureHandle fileTexture{ AssetsService::GetInstance()->LoadAsset<Texture>( fileTextureDesc, file ) };

        // Load folder icon
        Path_T folder{ PathBuilder()
                               .WithPath( m_AssetsRootDirectory.string() )
                               .WithPath( "folder0.png" )
                               .Build() };
        constexpr TextureLoadDescription folderTextureDesc{
            .Type{ TextureType::TEXTURE_2D },
        };
        TextureHandle folderTexture{ AssetsService::GetInstance()->LoadAsset<Texture>( folderTextureDesc ) };

        // The sampler for these two is kind of straightforward
        fileTexture->SetSampler( m_Device->GetDummyResource<Sampler>() );
        folderTexture->SetSampler( m_Device->GetDummyResource<Sampler>() );

        RefAny imguiFileHandle{ m_Device->CreateImguiTextureHandle(fileTexture) };
        RefAny imguiFolderHandle{ m_Device->CreateImguiTextureHandle(fileTexture) };

        m_TextureHandles.emplace( std::make_pair( TextureIconType::ICON_FILE, imguiFileHandle ) );
        m_TextureHandles.emplace( std::make_pair( TextureIconType::ICON_FOLDER, imguiFolderHandle ) );
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
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

            // Show folders only in the side tree?
            ImGuiUtils::CheckBox( "##ShowDirectoriesOnly", m_ShowFoldersOnlyInDirectoryTree );
            ImGui::SameLine();
            ImGui::Text( "Show directories only in side view" );

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

            ImGui::PushFont( ImGuiService::GetInstance()->GetFonts()[2] );
            if ( ImGui::Button( fmt::format( "{}", ICON_MD_CHEVRON_LEFT ).c_str() ) ) {
                m_ForwardDirectory = m_DirectoryStack.top();
                m_DirectoryStack.pop();

                m_CurrentDirectory = m_DirectoryStack.top();
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }
            ImGui::PopFont();

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
                m_DirectoryStack.push( m_ForwardDirectory );
                m_CurrentDirectory = m_ForwardDirectory;

                m_ForwardDirectory = Path_T{};
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
        {
            if ( ImGui::Button( fmt::format( "{}", ICON_MD_HOME ).c_str() ) ) {
                m_CurrentDirectory = m_ProjectRoot;
                m_ForwardDirectory = Path_T{};

                m_DirectoryStack = {};
                m_DirectoryStack.push( m_ProjectRoot );
            }
            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }
        }

        ImGui::SameLine();

        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_FOLDER ).c_str() );

        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
        ImGui::PushStyleColor( ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 0.16f, 0.16f, 0.16f, 0.5f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, { 0.0f, 0.0f, 0.0f, 0.0f } );

        // Directory buttons
        bool wantOpenDir{ false };
        auto pathIt{ m_DirectoryStack.top() };

        for ( ; pathIt != m_DirectoryStack.end(); ++pathIt ) {
            ImGui::SameLine();
            if ( ImGui::Button( pathIt->stem().string().c_str() ) ) {
                m_CurrentDirectory = *pathIt;
                m_ForwardDirectory = Path_T{};
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

    auto ContentBrowserPanel::DrawSideView() const -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_FramePadding |
                                                    ImGuiTreeNodeFlags_SpanFullWidth };

        for ( auto& entry: std::filesystem::directory_iterator( m_CurrentDirectory ) ) {
            if ( entry.is_directory() ) {
                if ( ImGui::TreeNodeEx( entry.path().string().c_str(), treeNodeFlags, "%s", fmt::format( "{} {}", ICON_MD_FOLDER, entry.path().stem().string() ).c_str() ) ) {

                    ImGui::TreePop();
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }
            }
        }
    }

    auto ContentBrowserPanel::DrawMainBody() -> void {
        DrawCurrentDirItems();
    }


    auto ContentBrowserPanel::OnUpdate( float timeStep ) -> void {
        if ( m_PanelIsVisible ) {
            static constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_None };
            static constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable |
                                                         ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedSame };

            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

            DrawHeader();

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::Separator();

            const ImVec2 availableRegion{ ImGui::GetContentRegionAvail() };

            if ( ImGui::BeginTable( "ContentBrowserMainViewTable", 2, tableFlags, availableRegion ) ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                DrawSideView();

                ImGui::TableNextColumn();
                DrawMainBody();

                ImGui::EndTable();
            }

            OnRightClick();

            ImGui::End();
        }
    }

    auto ContentBrowserPanel::DrawProjectDirTree( const Path_T& root ) const -> void {
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
        Path_T directoryToOpen{ m_CurrentDirectory };

        constexpr float padding{ 15.0f };
        const float cellSize{ m_ThumbnailSize + padding };

        const float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount{ static_cast<int>( panelWidth / cellSize ) };
        if ( columnCount < 1 ) {
            columnCount = 1;
        }

        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "ContentBrowserCurrentDir", columnCount, flags ) ) {
            ImGui::TableNextRow();
            for ( auto& entry: std::filesystem::directory_iterator( m_CurrentDirectory ) ) {
                ImGui::TableNextColumn();

                ImTextureID icon{};
                std::string fileType{};

                if ( entry.is_directory() ) {
                    icon = m_TextureHandles[TextureIconType::ICON_FOLDER];
                    fileType = "Folder";
                } else {
                    // find type (texture, material, text file) file now for simplicity
                    icon = m_TextureHandles[TextureIconType::ICON_FILE];
                    fileType = "File";
                }

                ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0, 0, 0, 0 ) );
                if ( ImGui::ImageButton( entry.path().string().c_str(), icon, ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } ) ) {
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

        if ( ImGui::BeginPopupContextWindow( "ContentBrowserPopup" ) ) {

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
                        std::filesystem::create_directory( m_CurrentDirectory / Path_T{ buffer.data() } );
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