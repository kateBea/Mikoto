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

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <imgui.h>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

#include <Core/Platform.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/ImGuiVulkanBackend.hh>

#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <ImGui/ImGuiD3D11Backend.hh>
#include <ImGui/ImGuiD3D12Backend.hh>
#endif

namespace mikoto::gui {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    static auto ThemeDarkModeAlt() -> void {
        // Setup Dear ImGui style
        ImGuiStyle &style{ ImGui::GetStyle() };

        style.Colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

        // Headers
        style.Colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Buttons
        style.Colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Frame BG
        style.Colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Tabs
        style.Colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
        style.Colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

        // Title
        style.Colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = .5f;
        style.GrabRounding = .5f;
        style.ChildRounding = .5f;
        style.WindowRounding = .5f;
        style.PopupRounding = .5f;
        style.ScrollbarRounding = .5f;
        style.TabRounding = .5f;
    }

    static auto ThemeDarkModeDefault() -> void {
        // Setup Dear ImGui style
        ImGuiStyle &style{ ImGui::GetStyle() };

        style.Colors[ImGuiCol_TitleBg] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4( 0.2f, 0.2f, 0.2f, 1.0f );
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );

        style.Colors[ImGuiCol_ResizeGrip] = ImVec4( 0.01f, 0.01f, 0.01f, 0.6f );
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.01f, 0.01f, 0.01f, 0.5f );
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.01f, 0.01f, 0.01f, 0.5f );

        style.Colors[ImGuiCol_Tab] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_Button] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_ButtonActive] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );

        style.Colors[ImGuiCol_TabHovered] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_TabActive] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4( 0.20f, 0.20f, 0.20f, 1.0f );

        style.Colors[ImGuiCol_Header] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_HeaderActive] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );

        style.Colors[ImGuiCol_MenuBarBg] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_FrameBg] = ImVec4( 0.08f, 0.08f, 0.08f, 1.0f );
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.092f, 0.092f, 0.092f, 1.0f );
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4( 0.08f, 0.08f, 0.08f, 1.0f );

        style.Colors[ImGuiCol_CheckMark] = ImVec4( 0.08f, 0.08f, 0.08f, 1.0f );
        style.Colors[ImGuiCol_CheckboxSelectedBg] = ImVec4( 0.08f, 0.08f, 0.08f, 1.0f );

        //style.Colors[ImGuiCol_Border] = ImVec4( 0.08f, 0.08f, 0.08f, 1.0f );
        // style.Colors[ImGuiCol_BorderShadow] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_SliderGrab] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_WindowBg] = ImVec4( 0.139f, 0.137f, 0.137f, 1.0f );
        style.Colors[ImGuiCol_ChildBg] = ImVec4( 0.139f, 0.137f, 0.137f, 0.00f );
        style.Colors[ImGuiCol_CheckMark] = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );

        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = 3.5f;
        style.GrabRounding = 3.5f;
        style.ChildRounding = 3.5f;
        style.WindowRounding = 3.5f;
        style.PopupRounding = 3.5f;
        style.ScrollbarRounding = 3.5f;
        style.TabRounding = 3.5f;
        style.MenuItemRounding = 3.5f;
        style.SelectableRounding = 3.5f;
    }

    ImGuiService::ImGuiService( const ImGuiServiceDescription &options )
        : mDevice{ options.mDevice },
         mImGuiFilesRootDir{ PathBuilder()
            .SetPath( "Resources" )
            .SetPath( "ImGui" )
            .Build() },
        mFontsRootDir{ PathBuilder()
            .SetPath( "Resources" )
            .SetPath( "Fonts" )
            .Build() },
        mBackendApi{ options.mApi },
        mWindow{ options.mWindow }
    {}

    auto ImGuiService::SetThemeDarkModeDefault() -> void {
        ThemeDarkModeDefault();
    }

    auto ImGuiService::SetThemeDarkModeAlt() -> void {
        ThemeDarkModeAlt();
    }

    auto ImGuiService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing ImGuiService...");

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO &io{ ImGui::GetIO() };
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport / Platform Windows

        // When viewports are enabled, we tweak WindowRounding/WindowBg
        // so platform windows can look identical to regular ones.
        ImGuiStyle& style{ ImGui::GetStyle() };

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.1f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Configure ImGui Style
        ImGui::StyleColorsDark();
        ThemeDarkModeDefault();

        // FontAwesome fonts need to have their sizes
        // reduced by 2.0f/3.0f in order to align correctly
        constexpr float iconFontSize{ kFontBaseSize * 1.01f };

        const Path path{ PathBuilder()
            .SetPath( mFontsRootDir.GetC_Str() )
            .SetPath( "JetBrains_Mono,Lexend,Noto_Sans_JP,Source_Code_Pro" )
            .SetPath( "JetBrains_Mono" )
            .SetPath( "static" )
            .SetPath( "JetBrainsMono-ExtraLight.ttf" )
            .Build() };

        // Add the main font
        AddFont(kFontBaseSize, path, nullptr, io.Fonts->GetGlyphRangesDefault() );

        // Config for japanese characters
        ImFontConfig jpConfig{};
        jpConfig.MergeMode = true;
        jpConfig.OversampleH = 2;
        jpConfig.OversampleV = 2;
        jpConfig.PixelSnapH = false;
        AddFont(kFontBaseSize, path, MKT_ADDRESSOF( jpConfig ), io.Fonts->GetGlyphRangesJapanese() );

        const Path fontPath{
            PathBuilder()
            .SetPath( mImGuiFilesRootDir.GetC_Str() )
            .Build() };

        // Made static because ImGui does not extend lifetime
        static constexpr eastl::array<ImWchar, 3> iconRanges1{ ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        static const auto faRegular{
            PathBuilder()
                .SetPath( fontPath.GetC_Str() )
                .SetPath( FONT_ICON_FILE_NAME_FAS )
                .Build() };

        AddIconFont( iconFontSize, faRegular, iconRanges1 );

        // See https://react-icons.github.io/react-icons/icons?name=md for icon previews
        static constexpr eastl::array<ImWchar, 3> iconRanges2{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };
        static const auto materialIconsRegular{
            PathBuilder()
                .SetPath( fontPath.GetC_Str() )
                .SetPath( FONT_ICON_FILE_NAME_MD )
                .Build() };

        AddIconFont( iconFontSize, materialIconsRegular, iconRanges2 );

        static constexpr eastl::array<ImWchar, 3> iconRanges3{ ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };
        static const auto materialDesign{
            PathBuilder()
                .SetPath( fontPath.GetC_Str() )
                .SetPath( FONT_ICON_FILE_NAME_MDI )
                .Build() };

        AddIconFont( iconFontSize, materialDesign, iconRanges3 );

        InitImplementation();

        mIsInitialized = true;
    }

    auto ImGuiService::AddFont( float fontSize, const eastl::string &path, const ImFontConfig* config, const ImWchar* glyphRanges ) -> void {
        ImGuiIO &io{ ImGui::GetIO() };

        auto result{ io.Fonts->AddFontFromFileTTF(path.c_str(), fontSize, config, glyphRanges ) };

        // ImGui pushes new fonts into the io.Fonts array when we add them using MergeMode == true
        // see imgui_draw_.cpp ImFont* ImFontAtlas::AddFont(const ImFontConfig* font_cfg_in)
        // Also first font cannot have MergeMode == true
        if (mImGuiFonts.empty() || result && (config == nullptr || !config->MergeMode)) {
            mImGuiFonts.try_emplace( Path{ Path{ path }.GetAbsolute() }, as<i8>( mImGuiFonts.size() ) );
        }
    }

    auto ImGuiService::AddIconFont( const float fontSize, const eastl::string &path, const eastl::array<ImWchar, 3> &iconRanges ) -> void {
        ImFontConfig config{};
        config.MergeMode = true;
        config.GlyphMinAdvanceX = 4.0f;
        config.PixelSnapH = true;
        config.GlyphOffset.y = 4.0f;
        config.GlyphOffset.x = 0.0f;
        config.OversampleH = config.OversampleV = 3.0f;
        config.SizePixels = 12.0f;

        AddFont(fontSize, path, std::addressof( config ), iconRanges.data() );
    }

    auto ImGuiService::SetImGuiBackGroundClearColor( const float4 &color ) -> void {
        mImplementation->SetClearColor( color );
    }

    auto ImGuiService::GetTextureID( TextureHandle texture ) -> ImTextureID {
        return GetTextureID(texture.GetRaw());
    }

    auto ImGuiService::GetTextureID( const ITexture *texture ) -> ImTextureID {
        return mImplementation->ConstructImGuiTextureID( texture );
    }

    auto ImGuiService::GetBackend() -> ImGuiBackend * {
        return mImplementation.get();
    }

    auto ImGuiService::GetBackend() const -> const ImGuiBackend * {
        return mImplementation.get();
    }

    auto ImGuiService::PushFont( eastl::string_view str ) -> ImGuiScopedTextFont {
        MKT_BEGIN_PROFILER_NAMED();

        auto it{ mImGuiFonts.find( Path{ Path{ str }.GetAbsolute() } ) };
        FileHandle fontFile{ FileService::Get()->LoadFile( Path{ str } ) };

        if ( it == mImGuiFonts.end() ) {
            if (fontFile.IsEmpty()) {
                MKT_CORE_LOGGER_WARN( "ImGuiService::PushFont - Failed to load font at {}", str );
                return ImGuiScopedTextFont( ImGuiScopedTextFont::Invalid );
            }

            AddFont( kFontBaseSize, fontFile->GetPath() );
        }

        MKT_ASSERT( fontFile, "Font File does not exist" );

        return ImGuiScopedTextFont{ mImGuiFonts.at( fontFile->GetPath() ) };
    }


    auto ImGuiService::InitImplementation() -> void {
        ImGuiIO &io{ ImGui::GetIO() };

        // Load ini file (static because IniFilename is const char*)
        // and it will not extend iniFilePath lifetime
        static const Path iniFilePath{
            PathBuilder()
                .SetPath( mImGuiFilesRootDir.GetC_Str() )
                .SetPath( "imgui.ini" )
                .Build() };

        io.IniFilename = iniFilePath.GetC_Str();

        // Create implementation
        const ImGuiBackendCreateInfo imGuiBackendCreateInfo{
            .mWindow = mWindow,
            .mDevice = mDevice,
            .mApi = mBackendApi };
        mImplementation = ImGuiBackend::Create( imGuiBackendCreateInfo );

        // Initialize the implementation
        if ( mImplementation ) {
            mImplementation->Init();
        } else {
            MKT_CORE_LOGGER_ERROR( "Failed to initialize an ImGui backend!" );
        }
    }

    auto ImGuiBackend::Create( const ImGuiBackendCreateInfo &info ) -> eastl::unique_ptr<ImGuiBackend> {
        switch ( info.mApi ) {
            case GraphicsAPI::eVulkan:
                return eastl::make_unique<ImGuiVulkanBackend>( info );
#if defined( MIKOTO_PLATFORM_WINDOWS )
            case GraphicsAPI::eD3D12:
                return eastl::make_unique<ImGuiD3D12Backend>( info );
            case GraphicsAPI::eD3D11:
                return eastl::make_unique<ImGuiD3D11Backend>( info );
#endif
            default:;
        }

        return nullptr;
    }

    auto ImGuiService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down ImGuiService..." );

        mImplementation->Shutdown();
        mImplementation.reset();

        ImGui::DestroyContext();
    }

    auto ImGuiService::EndFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mImplementation->EndFrame();
    }

    auto ImGuiService::PrepareFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mImplementation->BeginFrame();
    }

    auto ImGuiService::GetFinalComposition() const -> TextureHandle {
        return mImplementation->GetFinalComposition();
    }

}// namespace Mikoto