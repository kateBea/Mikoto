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

#include <string>
#include <exception>
#include <filesystem>

#include <EASTL/sort.h>

#include <yaml-cpp/yaml.h>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/LayoutsPanel.hh>

namespace mikoto::editor {
    LayoutsPanel::LayoutsPanel( const LayoutsPanelCreateInfo& info )
        : Panel{ "Layouts", "Layouts", info.mGroup, info.mIsToggleable },
          mCallbacks{ info } {
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_DASHBOARD, mPanelName );
        mPanelIsVisible = true;
        const auto* ini{ ImGui::GetIO().IniFilename };
        mDirectory = filesystem::Path{ ( ini ? std::filesystem::path{ ini }.parent_path() : std::filesystem::path{ "Resources/ImGui" } ) / "Layouts" }.ToAbsolute();
        RefreshLayouts();
    }

    auto LayoutsPanel::RefreshLayouts() -> void {
        try {
            mLayouts.clear();
            const auto directory{ mDirectory.GetPathTyped<std::filesystem::path>() };

            if ( !std::filesystem::exists( directory ) ) {
                return;
            }

            for ( const auto& entry: std::filesystem::directory_iterator{ directory } ) {
                if ( entry.is_regular_file() && entry.path().extension() == ".yaml" ) {
                    mLayouts.emplace_back( entry.path() );
                }
            }

            eastl::sort( mLayouts.begin(), mLayouts.end(), []( const auto& left, const auto& right ) {
                return left.GetPath() < right.GetPath();
            } );
        } catch ( const std::exception& error ) {
            mError = error.what();
        }
    }

    auto LayoutsPanel::SaveLayout() -> void {
        eastl::string name{ mName.data() };
        const auto first{ name.find_first_not_of( " \t\r\n" ) };

        if ( first == eastl::string::npos || name.find_first_of( "<>:\"/\\|?*" ) != eastl::string::npos ) {
            MKT_THROW_INVALID_ARGUMENT( MKT_LOC( "layouts_invalid_name" ).c_str() );
        }

        name = name.substr( first, name.find_last_not_of( " \t\r\n" ) - first + 1 );

        if ( name.back() == '.' ) {
            MKT_THROW_INVALID_ARGUMENT( MKT_LOC( "layouts_invalid_name" ).c_str() );
        }

        const auto native{ mDirectory.GetPathTyped<std::filesystem::path>() / std::filesystem::u8path( ( name + ".yaml" ).c_str() ) };

        if ( std::filesystem::exists( native ) ) {
            MKT_THROW_INVALID_ARGUMENT( MKT_LOC( "layouts_name_exists" ).c_str() );
        }

        std::filesystem::create_directories( native.parent_path() );
        size_t size{};
        const char* settings{ ImGui::SaveIniSettingsToMemory( &size ) };
        YAML::Node document{};
        document["Name"] = name.c_str();
        document["ImGui"] = std::string{ settings, size };

        for ( const auto& entry: mCallbacks.mCaptureVisibility() ) {
            const auto& panel{ entry.first };
            const auto& visible{ entry.second };

            document["Panels"][panel.c_str()] = visible;
        }

        YAML::Emitter output{};
        output << document;
        auto file{ filesystem::FileService::Get()->CreateNewFile( filesystem::Path{ native } ) };

        if ( !file ) {
            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "layouts_cannot_save" ).c_str() );
        }

        file->Write( eastl::string_view{ output.c_str(), output.size() } );
        mActiveLayout = name;
        mError.clear();
        RefreshLayouts();
    }

    auto LayoutsPanel::LoadLayout( const filesystem::Path& path ) -> void {
        auto file{ filesystem::FileService::Get()->LoadFile( path ) };

        if ( !file ) {
            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "layouts_cannot_load" ).c_str() );
        }

        file->UpdateContentsFromDisk();
        const auto document{ YAML::Load( file->GetContentsString().c_str() ) };

        if ( !document.IsMap() || !document["Name"].IsScalar() || !document["ImGui"].IsScalar() || !document["Panels"].IsMap() ) {
            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "layouts_cannot_load" ).c_str() );
        }

        const auto settings{ document["ImGui"].as<std::string>() };
        const auto name{ document["Name"].as<std::string>() };

        if ( settings.empty() ) {
            MKT_THROW_RUNTIME_ERROR( MKT_LOC( "layouts_cannot_load" ).c_str() );
        }

        LayoutPanelVisibility visibility{};

        for ( const auto& entry: document["Panels"] ) {
            visibility.emplace_back( entry.first.as<std::string>().c_str(), entry.second.as<bool>() );
        }
        // Queue both halves of the workspace; active panel windows finish this frame first.
        imgui::ImGuiService::Get()->QueueLayoutRestore( eastl::string{ settings.data(), settings.size() } );
        mCallbacks.mRestoreVisibility( eastl::move( visibility ) );
        mActiveLayout = name.c_str();
        mError.clear();
    }

    auto LayoutsPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 560, 400 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        mPanelIsHovered = ImGui::IsWindowHovered();
        mPanelIsFocused = ImGui::IsWindowFocused();
        bool create{};
        const float toolbarHeight{ ImGui::GetFrameHeight() + 8.0f };
        const ImVec2 toolbarPosition{ ImGui::GetCursorScreenPos() };
        const float toolbarWidth{ ImGui::GetContentRegionAvail().x };
        ImGui::GetWindowDrawList()->AddRectFilled( toolbarPosition,
            { toolbarPosition.x + toolbarWidth, toolbarPosition.y + toolbarHeight },
            ImGui::GetColorU32( ImGuiCol_FrameBg ), 6.0f );
        ImGui::GetWindowDrawList()->AddRect( toolbarPosition,
            { toolbarPosition.x + toolbarWidth, toolbarPosition.y + toolbarHeight },
            ImGui::GetColorU32( ImGuiCol_Border ), 6.0f );
        ImGui::SetCursorScreenPos( { toolbarPosition.x + 4.0f, toolbarPosition.y + 4.0f } );
        ImGui::BeginGroup();
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0, 0, 0, 0 } );

        try {
            create = ImGui::Button( ( eastl::string{ ICON_MD_ADD " " } + MKT_LOC( "layouts_new" ) ).c_str() );
            ImGui::SameLine();

            if ( ImGui::Button( ( eastl::string{ ICON_MD_REFRESH " " } + MKT_LOC( "layouts_reset" ) ).c_str() ) ) {
                mCallbacks.mResetLayout();
                mActiveLayout.clear();
            }

            ImGui::SameLine();
            const float folderWidth{ ImGui::CalcTextSize( ICON_MD_FOLDER_OPEN ).x + ImGui::GetStyle().FramePadding.x * 2.0f };
            ImGui::SetCursorScreenPos( { glm::max( ImGui::GetCursorScreenPos().x,
                                             toolbarPosition.x + toolbarWidth - folderWidth - 4.0f ),
                toolbarPosition.y + 4.0f } );

            if ( ImGui::Button( ICON_MD_FOLDER_OPEN "##LayoutsFolder" ) ) {
                std::filesystem::create_directories( mDirectory.GetPathTyped<std::filesystem::path>() );
                filesystem::OpenInExplorer( mDirectory );
                RefreshLayouts();
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "layouts_open_folder" ).c_str() );
        } catch ( const std::exception& error ) {
            mError = error.what();
        }

        ImGui::PopStyleColor();
        ImGui::EndGroup();
        ImGui::SetCursorScreenPos( { toolbarPosition.x, toolbarPosition.y + toolbarHeight + ImGui::GetStyle().ItemSpacing.y } );

        if ( create ) {
            mName.fill( 0 );
            mError.clear();
            ImGui::OpenPopup( "##NewEditorLayout" );
        }

        if ( ImGui::BeginPopup( "##NewEditorLayout" ) ) {
            ImGui::TextUnformatted( MKT_LOC( "layouts_name" ).c_str() );
            ImGui::SetNextItemWidth( 240.0f );
            const bool enter{ ImGui::InputText( "##LayoutName", mName.data(), mName.size(), ImGuiInputTextFlags_EnterReturnsTrue ) };

            if ( ImGui::Button( MKT_LOC( "layouts_save" ).c_str() ) || enter ) {
                try {
                    SaveLayout();
                    ImGui::CloseCurrentPopup();
                } catch ( const std::exception& error ) {
                    mError = error.what();
                }
            }

            ImGui::EndPopup();
        }

        if ( !mError.empty() ) {
            ImGui::TextWrapped( "%s", mError.c_str() );
        }

        ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 5.0f );
        auto background{ ImGui::GetStyleColorVec4( ImGuiCol_Header ) };
        background.w *= 0.45f;
        ImGui::PushStyleColor( ImGuiCol_ChildBg, background );
        const bool showLayouts{ ImGui::BeginChild( "##SavedLayouts", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding ) };

        if ( showLayouts && mLayouts.empty() ) {
            const auto available{ ImGui::GetContentRegionAvail() };
            const float lineHeight{ ImGui::GetFrameHeight() };
            const float textWidth{ glm::max( 1.0f, available.x - ImGui::GetStyle().FramePadding.x * 2.0f ) };
            const auto hint{ MKT_LOC( "layouts_empty_hint" ) };
            const float hintHeight{ ImGui::CalcTextSize( hint.c_str(), nullptr, false, textWidth ).y + ImGui::GetStyle().FramePadding.y * 2.0f };
            const float contentHeight{ lineHeight * 2.0f + hintHeight + ImGui::GetStyle().ItemSpacing.y * 2.0f };
            ImGui::SetCursorPosY( ImGui::GetCursorPosY() + glm::max( 0.0f, ( available.y - contentHeight ) * 0.5f ) );
            imgui::CenteredText( ICON_MD_DASHBOARD, available.x, lineHeight );
            imgui::CenteredText( MKT_LOC( "layouts_empty" ).c_str(), available.x, lineHeight );
            imgui::CenteredText( hint.c_str(), available.x, hintHeight, true );
        }
        else if ( showLayouts ) {
            filesystem::Path removed{};

            for ( const auto& path: mLayouts ) {
                const eastl::string name{ filesystem::Path{ path.GetPathTyped<std::filesystem::path>().stem() }.GetPath() };
                ImGui::PushID( path.GetC_Str() );

                if ( ImGui::Selectable( ( eastl::string{ ICON_MD_DASHBOARD "  " } + name ).c_str(), mActiveLayout == name ) ) {
                    try {
                        LoadLayout( path );
                    } catch ( const std::exception& error ) {
                        mError = error.what();
                    }
                }

                if ( ImGui::BeginPopupContextItem() ) {
                    if ( ImGui::MenuItem( MKT_LOC( "layouts_delete" ).c_str() ) ) {
                        removed = path;
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            if ( !removed.IsEmpty() ) {
                try {
                    std::filesystem::remove( removed.GetPathTyped<std::filesystem::path>() );

                    if ( mActiveLayout == filesystem::Path{ removed.GetPathTyped<std::filesystem::path>().stem() }.GetPath() ) {
                        mActiveLayout.clear();
                    }

                    RefreshLayouts();
                } catch ( const std::exception& error ) {
                    mError = error.what();
                }
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::End();
    }
}// namespace mikoto::editor
