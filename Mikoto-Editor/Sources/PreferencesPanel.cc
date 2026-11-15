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

#include <utility>
#include <array>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <string_view>

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Platform/ActionManager.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/ViewportPanel.hh>
#include <Panels/SettingsPanel.hh>
#include <Panels/PreferencesPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;

    struct ToolbarButtonPreference {
        core::cstr mKey{};
        core::cstr mLabelKey{};
        bool EditorState::ToolbarSettings::* mVisible{};
    };

    static constexpr std::array<ToolbarButtonPreference, 9> kToolbarButtonPreferences{ { { "save", "editor_layer_save", &EditorState::ToolbarSettings::mSave },
        { "open", "editor_layer_open", &EditorState::ToolbarSettings::mOpen },
        { "game", "editor_layer_game", &EditorState::ToolbarSettings::mGame },
        { "renderer", "editor_layer_renderer", &EditorState::ToolbarSettings::mRenderer },
        { "stats", "editor_layer_stats", &EditorState::ToolbarSettings::mStats },
        { "preferences", "editor_layer_preferences", &EditorState::ToolbarSettings::mPreferences },
        { "play", "viewport_play", &EditorState::ToolbarSettings::mPlay },
        { "simulate", "viewport_simulate", &EditorState::ToolbarSettings::mSimulate },
        { "stop", "viewport_stop", &EditorState::ToolbarSettings::mStop } } };

    static auto GetBundledInterfaceFonts() -> eastl::vector<platform::InstalledFontFamily> {
        // The first bundled family is used for new and reset preferences.
        return {
            { .mFamily = "Rajdhani", .mPath = "Resources/Fonts/Fira_Sans,Rajdhani/Rajdhani/Rajdhani-Regular.ttf" },
            { .mFamily = "Fira Sans" },
            { .mFamily = "Lexend", .mPath = "Resources/Fonts/JetBrains_Mono,Lexend,Noto_Sans_JP,Source_Code_Pro/Lexend/static/Lexend-Regular.ttf" },
            { .mFamily = "JetBrains Mono", .mPath = "Resources/Fonts/JetBrainsMono/fonts/ttf/JetBrainsMono-Regular.ttf" },
            { .mFamily = "Noto Sans JP", .mPath = "Resources/Fonts/Noto_Sans_JP/static/NotoSansJP-Regular.ttf" },
            { .mFamily = "Inter", .mPath = "Resources/Fonts/Inter/static/Inter-Regular.ttf" }
        };
    }


    PreferencesPanel::PreferencesPanel( const PreferencesPanelCreateInfo& info )
        : Panel{ "Preferences", "Preferences", info.mGroup, info.mIsToggleable },
          mState{ info.mState },
          mViewportPanel{ info.mViewportPanel },
          mCameraPanel{ info.mCameraPanel },
          mActionManager{ info.mActions },
          mOriginalStyle{ ImGui::GetStyle() } {
        std::filesystem::path directory;
#if defined( _WIN32 )
        if ( const char* path{ std::getenv( "APPDATA" ) } ) {
            directory = path;
        }
#else
        if ( const char* path{ std::getenv( "XDG_CONFIG_HOME" ) } ) {
            directory = path;
        }
        else if ( const char* path{ std::getenv( "HOME" ) } ) {
            directory = std::filesystem::path{ path } / ".config";
        }
#endif
        if ( directory.empty() ) {
            directory = std::filesystem::temp_directory_path();
        }

        mPath = filesystem::Path{ directory / "Mikoto" / "preferences.toml" };
        mFontChoices = GetBundledInterfaceFonts();
        mBundledFontCount = mFontChoices.size();
        mPanelIsVisible = false;
        Load();
    }

    auto PreferencesPanel::Load() -> void {
        if ( !filesystem::IsRegularFile( mPath ) ) {
            return;
        }

        const auto result{ toml::parse_file( mPath.GetC_Str() ) };

        if ( result.failed() ) {
            mStatus = MKT_LOC( "preferences_invalid_file" ).c_str();
            return;
        }

        const auto& table{ result.table() };
        mTheme = glm::clamp( table["appearance"]["theme"].value_or( 0 ), 0, 7 );

        const auto family{ table["appearance"]["font_family"].value_or( std::string_view{ "" } ) };
        if ( !family.empty() ) {
            const auto matchesFamily = [&]( const auto& choice ) {
                return eastl::string_view{ choice.mFamily } == eastl::string_view{ family.data(), family.size() };
            };
            auto font{ eastl::find_if( mFontChoices.begin(), mFontChoices.end(), matchesFamily ) };

            // A saved system selection needs its local font file before the first switch.
            if ( font == mFontChoices.end() ) {
                LoadSystemFonts();
                font = eastl::find_if( mFontChoices.begin() + mBundledFontCount, mFontChoices.end(), matchesFamily );
            }

            if ( font != mFontChoices.end() ) {
                mFontIndex = core::as<core::usize>( font - mFontChoices.begin() );
            }
        }

        mTextScale = table["appearance"]["text_scale"].value_or( kDefaultTextScale );
        mTooltipDelay = table["appearance"]["tooltip_delay"].value_or( 0.4f );
        mTextScale = glm::clamp( mTextScale, 0.8f, 1.6f );
        mTooltipDelay = glm::clamp( mTooltipDelay, 0.0f, 2.0f );
        mCompact = table["appearance"]["compact"].value_or( kDefaultCompactSpacing );
        mState->mShowStatusDateTime = table["appearance"]["show_status_date_time"].value_or( true );

        for ( const auto& button: kToolbarButtonPreferences ) {
            mState->mToolbarSettings.*( button.mVisible ) = table["appearance"]["toolbar_buttons"][button.mKey].value_or( true );
        }

        mState->mHeaderGradientColor = EditorState::kDefaultHeaderGradientColor;
        // Older preferences store RGB only; keep the original opacity for them.
        if ( const auto* color{ table["appearance"]["header_gradient_color"].as_array() }; color && ( color->size() == 3 || color->size() == 4 ) ) {
            for ( core::usize channel{}; channel < color->size(); ++channel ) {
                const float value{ ( *color )[channel].value_or( EditorState::kDefaultHeaderGradientColor[channel] ) };
                mState->mHeaderGradientColor[channel] = glm::clamp( value, 0.0f, 1.0f );
            }
        }
        // Adopt the ocean-blue default for preferences that saved the old gold
        // default, while preserving custom colors and the chosen opacity.
        const core::float3 oldDefault{ 232.0f / 255.0f, 202.0f / 255.0f, 118.0f / 255.0f };

        if ( table["version"].value_or( 1 ) < 2 && glm::all( glm::lessThan(
                                                       glm::abs( core::float3{ mState->mHeaderGradientColor } - oldDefault ), core::float3{ 0.5f / 255.0f } ) ) ) {
            const float alpha{ mState->mHeaderGradientColor.a };
            mState->mHeaderGradientColor = EditorState::kDefaultHeaderGradientColor;
            mState->mHeaderGradientColor.a = alpha;
        }
        // Update the previous default tint without replacing custom user colors.
        const core::float4 previousBlue{ 22.0f / 255.0f, 79.0f / 255.0f, 122.0f / 255.0f, 175.0f / 255.0f };

        if ( table["version"].value_or( 1 ) < 3 && glm::all( glm::lessThan(
                                                       glm::abs( mState->mHeaderGradientColor - previousBlue ), core::float4{ 0.5f / 255.0f } ) ) ) {
            mState->mHeaderGradientColor = EditorState::kDefaultHeaderGradientColor;
        }

        mVsync = table["viewport"]["vsync"].value_or( true );
        mPerformanceOverlay = table["viewport"]["performance_overlay"].value_or( false );
        mUtilityOverlay = table["viewport"]["utility_overlay"].value_or( true );
        mState->mCenterPivot = table["viewport"]["center_pivot"].value_or( true );
        mState->mTransformChildren = table["viewport"]["transform_children"].value_or( true );

        using ToolsLayout = EditorState::ViewSettings::ToolsLayout;
        using OrientationAnchor = EditorState::ViewSettings::OrientationAnchor;

        mState->mViewSettings.mToolsLayout = static_cast<ToolsLayout>(
            glm::clamp( table["viewport"]["tools_layout"].value_or( as<i32>( ToolsLayout::eDefault ) ),
                as<i32>( ToolsLayout::eDefault ), as<i32>( ToolsLayout::eClassic ) ) );
        mState->mViewSettings.mOrientationAnchor = static_cast<OrientationAnchor>(
            glm::clamp( table["viewport"]["orientation_anchor"].value_or( as<i32>( OrientationAnchor::eBottomLeft ) ),
                as<i32>( OrientationAnchor::eBottomLeft ), as<i32>( OrientationAnchor::eTopRight ) ) );
        mState->mViewSettings.mClassicOrientationAnchor = static_cast<OrientationAnchor>(
            glm::clamp( table["viewport"]["classic_orientation_anchor"].value_or( as<i32>( OrientationAnchor::eBottomLeft ) ),
                as<i32>( OrientationAnchor::eBottomLeft ), as<i32>( OrientationAnchor::eTopRight ) ) );

        mState->mExternalEditor = filesystem::Path{ table["scripting"]["editor"].value_or( std::string{} ) };
    }

    auto PreferencesPanel::Save() -> void {
        try {
            toml::table toolbarButtons{};

            for ( const auto& button: kToolbarButtonPreferences ) {
                toolbarButtons.insert( button.mKey, mState->mToolbarSettings.*( button.mVisible ) );
            }

            const auto color{ mState->mHeaderGradientColor };
            toml::table table{
                { "version", 3 },
                { "appearance", toml::table{
                                    { "theme", mTheme },
                                    { "font_family", mFontChoices[mFontIndex].mFamily.c_str() },
                                    { "text_scale", mTextScale },
                                    { "tooltip_delay", mTooltipDelay },
                                    { "compact", mCompact },
                                    { "show_status_date_time", mState->mShowStatusDateTime },
                                    { "header_gradient_color", toml::array{ color.r, color.g, color.b, color.a } },
                                    { "toolbar_buttons", std::move( toolbarButtons ) } } },
                { "viewport", toml::table{
                                  { "vsync", mVsync },
                                  { "performance_overlay", mPerformanceOverlay },
                                  { "utility_overlay", mUtilityOverlay },
                                  { "center_pivot", mState->mCenterPivot },
                                  { "transform_children", mState->mTransformChildren },
                                  { "tools_layout", as<i32>( mState->mViewSettings.mToolsLayout ) },
                                  { "orientation_anchor", as<i32>( mState->mViewSettings.mOrientationAnchor ) },
                                  { "classic_orientation_anchor", as<i32>( mState->mViewSettings.mClassicOrientationAnchor ) } } },
                { "scripting", toml::table{ { "editor", mState->mExternalEditor.GetC_Str() } } }
            };
            std::ostringstream text;
            text << table;
            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( mPath ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open preferences file for writing." );
            }

            file->Write( text.str().c_str() );
            mDirty = false;
            mStatus = MKT_LOC( "preferences_saved" ).c_str();
        } catch ( const std::exception& error ) {
            mStatus = error.what();
        }
    }

    auto PreferencesPanel::ApplyPending() -> void {
        if ( !mPending ) {
            return;
        }
        // Called before any panels push style variables: never replace the
        // style mid-window or repeatedly multiply its spacing every frame.
        auto& style{ ImGui::GetStyle() };
        style = mOriginalStyle;

        if ( mTheme == 1 ) {
            ImGui::StyleColorsDark();
        }
        else if ( mTheme == 2 ) {
            ImGui::StyleColorsLight();
        }

        if ( mTheme == 3 ) {
            ImGui::StyleColorsClassic();
        }
        else if ( mTheme == 4 ) {
            ImGui::StyleColorsDark();
            imgui::ImGuiService::Get()->SetThemeDarkModeDefault();
        }
        else if ( mTheme == 5 ) {
            ImGui::StyleColorsDark();
            imgui::ImGuiService::Get()->SetThemeDarkModeAlt();
        }
        else if ( mTheme == 6 ) {
            ImGui::StyleColorsDark();
        }
        else if ( mTheme == 7 ) {
            ImGui::StyleColorsLight();
        }

        style.Colors[ImGuiCol_ModalWindowDimBg] = { 0.0f, 0.0f, 0.0f, 0.65f };
        style.FontScaleMain = mTextScale;

        if ( mFontIndex != mAppliedFontIndex ) {
            const auto& font{ mFontChoices[mFontIndex] };
            imgui::ImGuiService::Get()->QueueInterfaceFont( font.mPath, font.mFaceIndex );
            mAppliedFontIndex = mFontIndex;
        }

        style.HoverDelayNormal = mTooltipDelay;

        if ( mCompact ) {
            style.FramePadding.y = 2.0f;
            style.ItemSpacing.y = 4.0f;
        }

        renderer::RenderSystem::Get()->GetContext()->SetRefreshRate(
            mVsync ? renderer::rhi::RefreshRate::eSync : renderer::rhi::RefreshRate::eUnlimited );
        mViewportPanel->SetEnablePerformanceOverlay( mPerformanceOverlay );
        mViewportPanel->SetEnableUtilityOverlay( mUtilityOverlay );
        mPending = false;
    }

    auto PreferencesPanel::DrawShortcuts() const -> void {
        if ( !mActionManager ) {
            return;
        }

        struct ShortcutRow {
            eastl::string mShortcut{};
            eastl::string mDescription{};
        };
        eastl::vector<ShortcutRow> rows{};
        rows.reserve( mActionManager->GetBindings().size() );

        for ( const auto& entry: mActionManager->GetBindings() ) {
            const auto& shortcut{ entry.first };
            const auto& binding{ entry.second };

            if ( !binding.mDescription.empty() ) {
                rows.push_back( { shortcut.GetName(), MKT_LOC( binding.mDescription ) } );
            }
        }

        eastl::sort( rows.begin(), rows.end(), []( const auto& left, const auto& right ) {
            return left.mShortcut < right.mShortcut;
        } );

        if ( ImGui::BeginTable( "KeyboardShortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "preferences_shortcut" ).c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9.0f );
            ImGui::TableSetupColumn( MKT_LOC( "preferences_shortcut_description" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableHeadersRow();

            for ( const auto& row: rows ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( row.mShortcut.c_str() );
                ImGui::TableSetColumnIndex( 1 );
                ImGui::TextWrapped( "%s", row.mDescription.c_str() );
            }

            ImGui::EndTable();
        }
    }

    auto PreferencesPanel::OpenPlatformSettings() -> void {
        mCategory = kPlatformCategory;
        mSearchFilter.Clear();
        SetVisible( true );
    }

    auto PreferencesPanel::DrawPlatformSettings() -> void {
        ImGui::SeparatorText( MKT_LOC( "platform_title" ).c_str() );
        ImGui::TextWrapped( "%s", MKT_LOC( "platform_placeholder_help" ).c_str() );
        ImGui::Spacing();

        if ( ImGui::BeginTable( "PlatformProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            const char* labelKeys[]{ "platform_preview", "platform_graphics_api", "platform_architecture", "platform_configuration" };
            float labelWidth{ ImGui::CalcTextSize( "Android SDK / NDK" ).x };

            for ( const auto* key: labelKeys ) {
                labelWidth = eastl::max( labelWidth, ImGui::CalcTextSize( MKT_LOC( key ).c_str() ).x );
            }

            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, labelWidth + ImGui::GetStyle().ItemInnerSpacing.x );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );

            // Switching the preview only changes which future platform options are shown.
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "platform_preview" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            ImGui::SetNextItemWidth( -1.0f );
            const char* platforms[]{ "Windows", "Linux", "Android" };
            ImGui::Combo( "##PlatformPreview", &mPlatformPreview, platforms, IM_ARRAYSIZE( platforms ) );

            ImGui::BeginDisabled();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "platform_graphics_api" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "Vulkan" );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "platform_architecture" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( mPlatformPreview == 2 ? "ARM64" : "x86_64" );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "platform_configuration" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            ImGui::SetNextItemWidth( -1.0f );
            int configuration{ 1 };
            ImGui::Combo( "##PlatformConfiguration", &configuration, "Debug\0Release\0" );

            if ( mPlatformPreview == 2 ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( "Android SDK / NDK" );
                ImGui::TableSetColumnIndex( 1 );
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( MKT_LOC( "platform_not_configured" ).c_str() );
            }

            ImGui::EndDisabled();
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::BeginDisabled();
        bool packageAssets{ true };
        bool debugSymbols{};
        imgui::CheckBox( imgui::widget::LocalizedLabel( "platform_package_assets" ).c_str(), packageAssets );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "platform_debug_symbols" ).c_str(), debugSymbols );
        ImGui::EndDisabled();
    }

    auto PreferencesPanel::LoadSystemFonts() -> void {
        if ( mSystemFontsLoaded ) {
            return;
        }

        auto installed{ platform::GetInstalledFontFamilies() };
        eastl::sort( installed.begin(), installed.end(), []( const auto& lhs, const auto& rhs ) {
            return lhs.mFamily < rhs.mFamily;
        } );

        for ( auto& family: installed ) {
            const auto duplicate{ eastl::find_if( mFontChoices.begin(), mFontChoices.end(), [&]( const auto& choice ) {
                return choice.mFamily == family.mFamily;
            } ) };

            if ( duplicate == mFontChoices.end() ) {
                mFontChoices.push_back( eastl::move( family ) );
            }
        }

        mSystemFontsLoaded = true;
    }

    auto PreferencesPanel::DrawFontSelector() -> bool {
        const auto label{ imgui::widget::LocalizedLabel( "preferences_ui_font" ) };
        if ( !ImGui::BeginCombo( label.c_str(), mFontChoices[mFontIndex].mFamily.c_str(), ImGuiComboFlags_HeightLarge ) ) {
            return false;
        }

        if ( ImGui::IsWindowAppearing() ) {
            mFontFilter.Clear();
        }

        ImGui::SetNextItemWidth( -1.0f );
        if ( ImGui::InputTextWithHint( "##FontFilter", MKT_LOC( "preferences_search_filter" ).c_str(),
                 mFontFilter.InputBuf, IM_ARRAYSIZE( mFontFilter.InputBuf ) ) ) {
            mFontFilter.Build();
        }

        bool changed{};
        const auto drawFonts = [&]( core::usize first, core::usize end ) {
            for ( core::usize index{ first }; index < end; ++index ) {
                const auto& family{ mFontChoices[index].mFamily };
                if ( mFontFilter.PassFilter( family.c_str() ) && ImGui::Selectable( family.c_str(), index == mFontIndex ) ) {
                    changed = index != mFontIndex;
                    mFontIndex = index;
                }
            }
        };

        drawFonts( 0, mBundledFontCount );

#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( !mSystemFontsLoaded && ImGui::Button( imgui::widget::LocalizedLabel( "preferences_show_system_fonts" ).c_str() ) ) {
            LoadSystemFonts();
        }

        if ( mSystemFontsLoaded ) {
            if ( mBundledFontCount < mFontChoices.size() ) {
                ImGui::SeparatorText( MKT_LOC( "preferences_system_fonts" ).c_str() );
                drawFonts( mBundledFontCount, mFontChoices.size() );
            } else {
                ImGui::TextDisabled( "%s", MKT_LOC( "preferences_system_fonts_unavailable" ).c_str() );
            }
        }
#endif

        ImGui::EndCombo();
        return changed;
    }

    auto PreferencesPanel::OnRender( float ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 760.0f, 520.0f }, ImGuiCond_FirstUseEver );

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::SetNextItemWidth( -1.0f );
            const eastl::string searchText{ MKT_LOC( "preferences_search_filter" ) };
            const eastl::string formattedText{ string::Format( "{} {}...", ICON_MD_SEARCH, searchText.c_str() ) };

            if ( ImGui::InputTextWithHint( "##PreferencesPanelInputText", formattedText.c_str(),
                     mSearchFilter.InputBuf, IM_ARRAYSIZE( mSearchFilter.InputBuf ) ) ) {
                mSearchFilter.Build();
            }

            ImGui::Separator();
            ImGui::BeginChild( "Categories", { 150.0f, -65.0f }, ImGuiChildFlags_Borders );
            const char* categories[]{ MKT_LOC( "preferences_appearance" ).c_str(), MKT_LOC( "preferences_viewport" ).c_str(), MKT_LOC( "preferences_external_tools" ).c_str(), MKT_LOC( "preferences_shortcuts" ).c_str(), MKT_LOC( "preferences_assets" ).c_str(), MKT_LOC( "platform_title" ).c_str() };

            for ( int index{}; index < IM_ARRAYSIZE( categories ); ++index ) {
                ImGui::PushID( index );

                if ( ImGui::Selectable( categories[index], mCategory == index ) ) {
                    mCategory = index;
                }

                ImGui::PopID();
            }

            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild( "Properties", { 0.0f, -65.0f }, ImGuiChildFlags_Borders );
            const auto section = [&]( int category, const char* terms ) {
                return mSearchFilter.IsActive() ? mSearchFilter.PassFilter( terms ) : mCategory == category;
            };

            bool changed{};

            if ( section( 0, MKT_LOC( "preferences_appearance_search" ).c_str() ) ) {
                ImGui::SeparatorText( MKT_LOC( "preferences_appearance" ).c_str() );
                const char* themes[]{ "Mikoto", MKT_LOC( "preferences_theme_dark" ).c_str(),
                    MKT_LOC( "preferences_theme_light" ).c_str(), MKT_LOC( "preferences_theme_classic" ).c_str(),
                    MKT_LOC( "preferences_theme_dark_default" ).c_str(), MKT_LOC( "preferences_theme_dark_alternative" ).c_str(),
                    MKT_LOC( "preferences_theme_focused" ).c_str(), MKT_LOC( "preferences_theme_blindness" ).c_str() };
                changed |= ImGui::Combo( imgui::widget::LocalizedLabel( "preferences_theme" ).c_str(), &mTheme, themes, IM_ARRAYSIZE( themes ) );
                changed |= DrawFontSelector();
                changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "preferences_text_scale" ).c_str(), &mTextScale, 0.8f, 1.6f, "%.2f" );
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_compact_spacing" ).c_str(), mCompact );
                changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "preferences_tooltip_delay" ).c_str(), &mTooltipDelay, 0.0f, 2.0f, "%.2f s" );
                changed |= ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "preferences_header_gradient_color" ).c_str(), &mState->mHeaderGradientColor.x,
                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview );
                ImGui::SeparatorText( MKT_LOC( "preferences_status_bar" ).c_str() );
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_show_date_time" ).c_str(), mState->mShowStatusDateTime );

                ImGui::SeparatorText( MKT_LOC( "preferences_toolbar_buttons" ).c_str() );
                ImGui::PushID( "MainToolbarButtons" );

                for ( const auto& button: kToolbarButtonPreferences ) {
                    changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( button.mLabelKey ).c_str(), ( mState->mToolbarSettings.*( button.mVisible ) ) );
                }

                ImGui::PopID();
            }

            if ( section( 1, MKT_LOC( "preferences_viewport_search" ).c_str() ) ) {
                ImGui::SeparatorText( MKT_LOC( "preferences_viewport" ).c_str() );
                int toolsLayout{ core::as<int>( mState->mViewSettings.mToolsLayout ) };
                const eastl::array<const char*, 2> layouts{ MKT_LOC( "viewport_tools_layout_default" ).c_str(), MKT_LOC( "viewport_tools_layout_classic" ).c_str() };

                if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_tools_layout", "Viewport tools" ).c_str(), &toolsLayout, layouts.data(), core::as<int>( layouts.size() ) ) ) {
                    mState->mViewSettings.mToolsLayout = static_cast<EditorState::ViewSettings::ToolsLayout>( toolsLayout );
                    changed = true;
                }

                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_vertical_sync" ).c_str(), mVsync );
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_performance_overlay" ).c_str(), mPerformanceOverlay );
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_utility_toolbar" ).c_str(), mUtilityOverlay );
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_selection_center_pivot" ).c_str(), mState->mCenterPivot );
                ImGui::TextWrapped( MKT_LOC( "preferences_pivot_help" ).c_str() );

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "preferences_camera_settings" ).c_str() ) ) {
                    mCameraPanel->SetVisible( true );
                }

                ImGui::TextWrapped( MKT_LOC( "preferences_camera_help" ).c_str() );
            }

            if ( section( 2, MKT_LOC( "preferences_external_tools_search" ).c_str() ) ) {
                ImGui::SeparatorText( MKT_LOC( "preferences_external_tools" ).c_str() );
                ImGui::TextWrapped( MKT_LOC( "preferences_script_editor" ).c_str(), mState->mExternalEditor.IsEmpty() ? MKT_LOC( "preferences_default_editor" ).c_str() : mState->mExternalEditor.GetC_Str() );

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "preferences_choose_editor" ).c_str() ) ) {
                    const auto file{ filesystem::OpenFileDialog( {} ) };

                    if ( !file.IsEmpty() ) {
                        mState->mExternalEditor = file;
                        changed = true;
                    }
                }

                ImGui::SameLine();

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "preferences_use_vscode" ).c_str() ) ) {
                    mState->mExternalEditor = filesystem::Path{};
                    changed = true;
                }

#if defined( MIKOTO_PLATFORM_WINDOWS )
                if ( imgui::widget::AccentButton( imgui::widget::LocalizedLabel( "preferences_use_visual_studio" ).c_str() ) ) {
                    mState->mExternalEditor = filesystem::Path{ "Visual Studio" };
                    changed = true;
                }
#endif

                ImGui::BeginDisabled( !mState->mHasProject || mState->mScriptSolution.IsEmpty() );

                if ( imgui::widget::AccentButton( imgui::widget::LocalizedLabel( "preferences_open_script_project" ).c_str() ) ) {
                    if ( !filesystem::OpenInTextEditor( mState->mScriptSolution, mState->mExternalEditor ) ) {
                        mStatus = MKT_LOC( "preferences_script_project_open_failed" ).c_str();
                    }
                }

                ImGui::EndDisabled();
            }

            if ( section( 3, MKT_LOC( "preferences_shortcuts_search" ).c_str() ) ) {
                ImGui::SeparatorText( MKT_LOC( "preferences_keyboard_shortcuts" ).c_str() );
                DrawShortcuts();
                ImGui::TextWrapped( MKT_LOC( "preferences_shortcuts_help" ).c_str() );
            }

            if ( section( 4, MKT_LOC( "preferences_assets_search" ).c_str() ) ) {
                ImGui::SeparatorText( MKT_LOC( "preferences_project_assets" ).c_str() );
                const bool editable{ mState->mHasProject && !mState->mRuntimeActive &&
                                     mState->mGetTextureCompression && mState->mSetTextureCompression };
                bool compression{ mState->mGetTextureCompression && mState->mGetTextureCompression() };
                ImGui::BeginDisabled( !editable );

                if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "preferences_compress_textures" ).c_str(), compression ) ) {
                    // This is project data, not a global preferences.toml value.
                    mState->mSetTextureCompression( compression );
                }

                ImGui::EndDisabled();
                ImGui::TextWrapped( MKT_LOC( "preferences_compression_load_help" ).c_str() );
                ImGui::TextWrapped( MKT_LOC( "preferences_compression_save_help" ).c_str() );

                if ( !mState->mHasProject ) {
                    ImGui::TextDisabled( MKT_LOC( "preferences_project_required" ).c_str() );
                }
            }

            if ( section( kPlatformCategory, MKT_LOC( "platform_search" ).c_str() ) ) {
                DrawPlatformSettings();
            }

            mPending |= changed;
            mDirty |= changed;
            ImGui::EndChild();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "preferences_save" ).c_str() ) ) {
                Save();
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "preferences_reset_defaults" ).c_str() ) ) {
                mTheme = 0;
                mFontIndex = 0;
                mTextScale = kDefaultTextScale;
                mTooltipDelay = 0.4f;
                mCompact = kDefaultCompactSpacing;
                mState->mShowStatusDateTime = true;
                mState->mToolbarSettings = {};
                mState->mHeaderGradientColor = EditorState::kDefaultHeaderGradientColor;
                mVsync = true;
                mPerformanceOverlay = false;
                mUtilityOverlay = true;
                mState->mViewSettings.mToolsLayout = EditorState::ViewSettings::ToolsLayout::eDefault;
                mState->mViewSettings.mOrientationAnchor = EditorState::ViewSettings::OrientationAnchor::eBottomLeft;
                mState->mViewSettings.mClassicOrientationAnchor = EditorState::ViewSettings::OrientationAnchor::eBottomLeft;
                mState->mCenterPivot = true;
                mState->mTransformChildren = true;
                mState->mTransformEditRejected = false;
                mState->mExternalEditor = filesystem::Path{};
                mPending = mDirty = true;
            }

            ImGui::SameLine();
            ImGui::TextDisabled( "%s", mDirty ? MKT_LOC( "preferences_unsaved_changes" ).c_str() : "" );
            ImGui::TextWrapped( "%s", mStatus.empty() ? mPath.GetC_Str() : mStatus.c_str() );
        }

        ImGui::End();
    }
}
