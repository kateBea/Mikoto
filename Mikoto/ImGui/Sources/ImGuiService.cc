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

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <implot.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsFontAwesome6.h>
#include <ImGui/ImGuiVulkanBackend.hh>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <ImGui/ImGuiD3D11Backend.hh>
#include <ImGui/ImGuiD3D12Backend.hh>
#endif

#include <ImGui/ImGuiOpenGLBackend.hh>

namespace mikoto::imgui {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    static auto ThemeDarkModeAlt() -> void {
        MKT_BEGIN_PROFILER();
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
        style.Colors[ImGuiCol_TitleBg] = ImVec4{ 0.08f, 0.08f, 0.08f, 1.0f };
        style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg];
        style.Colors[ImGuiCol_TitleBgCollapsed] = style.Colors[ImGuiCol_TitleBg];

        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = 3.0f;
        style.GrabRounding = 2.0f;
        style.ChildRounding = .5f;
        style.WindowRounding = .5f;
        style.PopupRounding = .5f;
        style.ScrollbarRounding = .5f;
        style.TabRounding = .5f;
    }

    static auto ThemeDarkModeDefault() -> void {
        MKT_BEGIN_PROFILER();
        auto &style{ ImGui::GetStyle() };
        auto *colors{ style.Colors };

        // Neutral gray surfaces, with brighter shades for selection and keyboard focus.
        constexpr ImVec4 panel{ 0.19f, 0.19f, 0.19f, 1.0f };
        constexpr ImVec4 toolbar{ 0.15f, 0.15f, 0.15f, 1.0f };
        constexpr ImVec4 titleBar{ 0.10f, 0.10f, 0.10f, 1.0f };
        constexpr ImVec4 field{ 0.13f, 0.13f, 0.13f, 1.0f };
        constexpr ImVec4 selection{ 0.28f, 0.28f, 0.28f, 1.0f };
        constexpr ImVec4 selectionHovered{ 0.34f, 0.34f, 0.34f, 1.0f };
        constexpr ImVec4 accent{ 0.52f, 0.52f, 0.52f, 1.0f };

        colors[ImGuiCol_Text] = { 0.86f, 0.86f, 0.86f, 1.0f };
        colors[ImGuiCol_TextDisabled] = { 0.55f, 0.55f, 0.55f, 1.0f };
        colors[ImGuiCol_WindowBg] = panel;
        colors[ImGuiCol_ChildBg] = { 0, 0, 0, 0 };
        colors[ImGuiCol_PopupBg] = { 0.17f, 0.17f, 0.17f, 0.98f };
        colors[ImGuiCol_MenuBarBg] = toolbar;
        colors[ImGuiCol_TitleBg] = titleBar;
        colors[ImGuiCol_TitleBgActive] = titleBar;
        colors[ImGuiCol_TitleBgCollapsed] = titleBar;
        colors[ImGuiCol_Border] = { 0.0f, 0.0f, 0.0f, 1.0f };
        colors[ImGuiCol_BorderShadow] = { 0, 0, 0, 0 };

        colors[ImGuiCol_FrameBg] = field;
        // Checkboxes also use these states: keep checked frames visible on hover/press.
        colors[ImGuiCol_FrameBgHovered] = { 0.30f, 0.30f, 0.30f, 1.0f };
        colors[ImGuiCol_FrameBgActive] = { 0.27f, 0.27f, 0.27f, 1.0f };
        colors[ImGuiCol_Button] = selection;
        colors[ImGuiCol_ButtonHovered] = selectionHovered;
        colors[ImGuiCol_ButtonActive] = selection;
        colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
        colors[ImGuiCol_CheckboxSelectedBg] = selection;
        colors[ImGuiCol_SliderGrab] = { 0.48f, 0.48f, 0.48f, 1.0f };
        colors[ImGuiCol_SliderGrabActive] = accent;

        colors[ImGuiCol_Header] = selection;
        colors[ImGuiCol_HeaderHovered] = selectionHovered;
        colors[ImGuiCol_HeaderActive] = selection;
        colors[ImGuiCol_TableHeaderBg] = { 0.22f, 0.22f, 0.22f, 1.0f };
        colors[ImGuiCol_Tab] = toolbar;
        colors[ImGuiCol_TabHovered] = { 0.29f, 0.29f, 0.29f, 1.0f };
        colors[ImGuiCol_TabActive] = { 0.23f, 0.23f, 0.23f, 1.0f };
        colors[ImGuiCol_TabUnfocused] = toolbar;
        colors[ImGuiCol_TabUnfocusedActive] = panel;
        colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_Border];
        colors[ImGuiCol_TabDimmedSelectedOverline] = colors[ImGuiCol_Border];

        colors[ImGuiCol_ScrollbarBg] = toolbar;
        colors[ImGuiCol_ScrollbarGrab] = { 0.34f, 0.34f, 0.34f, 1.0f };
        colors[ImGuiCol_ScrollbarGrabHovered] = { 0.42f, 0.42f, 0.42f, 1.0f };
        colors[ImGuiCol_ScrollbarGrabActive] = { 0.48f, 0.48f, 0.48f, 1.0f };
        colors[ImGuiCol_Separator] = { 0.12f, 0.12f, 0.12f, 1.0f };
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_ResizeGrip] = { 0, 0, 0, 0 };
        colors[ImGuiCol_ResizeGripHovered] = selection;
        colors[ImGuiCol_ResizeGripActive] = accent;
        colors[ImGuiCol_NavCursor] = accent;
        colors[ImGuiCol_TextSelectedBg] = selection;
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_DockingPreview] = { accent.x, accent.y, accent.z, 0.45f };
        colors[ImGuiCol_DockingEmptyBg] = toolbar;
        colors[ImGuiCol_ModalWindowDimBg] = { 0, 0, 0, 0.65f };

        // Fields, buttons and checkbox squares share the viewport settings' height.
        // ImGui::GetFrameHeight() is font size plus twice the vertical padding.
        style.FramePadding = { 6.0f, 4.0f };
        style.ItemSpacing = { 8.0f, 6.0f };

        // Outline panels and tabs while keeping the controls inside them visually lighter.
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 0.0f;
        style.TabMinWidthBase = 96.0f;

        // Use small corner radii without rounding row selections or drag targets.
        style.FrameRounding = 4.0f;
        style.GrabRounding = 2.0f;
        style.ChildRounding = 0.0f;
        style.WindowRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 1.0f;
        style.TabRounding = 2.0f;
        style.MenuItemRounding = 0.0f;
        style.SelectableRounding = 0.0f;
        style.DragDropTargetRounding = 0.0f;
    }

    ImGuiService::ImGuiService( const ImGuiServiceDescription &options )
        : mWindow{ options.mWindow },
          mDevice{ options.mDevice },
          mFontsRootDir{ filesystem::PathBuilder()
              .SetPath( "Resources" )
              .SetPath( "Fonts" )
              .Build() },
          mImGuiFilesRootDir{ filesystem::PathBuilder()
              .SetPath( "Resources" )
              .SetPath( "ImGui" )
              .Build() },
          mBackendApi{ options.mApi } {
    }

    auto ImGuiService::SetThemeDarkModeDefault() -> void {
        MKT_BEGIN_PROFILER();
        ThemeDarkModeDefault();
    }

    auto ImGuiService::SetThemeDarkModeAlt() -> void {
        MKT_BEGIN_PROFILER();
        ThemeDarkModeAlt();
    }

    auto ImGuiService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing ImGuiService..." );

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();
        ImPlot::CreateContext();

        ImGuiIO &io{ ImGui::GetIO() };
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport / Platform Windows

        // Content clicks belong to controls and canvases; move windows from their title bars or dock tabs.
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        // When viewports are enabled, we tweak WindowRounding/WindowBg
        // so platform windows can look identical to regular ones.
        ImGuiStyle &style{ ImGui::GetStyle() };

        if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Configure ImGui Style
        ImGui::StyleColorsDark();
        ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = { 0.0f, 0.0f, 0.0f, 0.65f };
        ThemeDarkModeDefault();

        // Keep dock tabs and close buttons, without the window-menu arrow.
        style.WindowMenuButtonPosition = ImGuiDir_None;

        const filesystem::Path path{ filesystem::PathBuilder()
            .SetPath( mFontsRootDir.GetC_Str() )
            .SetPath( "Fira_Sans,JetBrains_Mono,Lexend,Noto_Sans_JP,Source_Code_Pro" )
            .SetPath( "Fira_Sans" )
            .SetPath( "FiraSans-Light.ttf" )
            .Build() };

        if ( auto* font{ AddInterfaceFont( path, 0 ) } ) {
            mImGuiFonts.try_emplace( filesystem::Path{ path.GetAbsolute() }, font );
        } else {
            io.Fonts->AddFontDefault();
        }

        // Register the console face before it can be requested from a docked window.
        const filesystem::Path consoleFontPath{ filesystem::PathBuilder()
            .SetPath( mFontsRootDir.GetC_Str() )
            .SetPath( "Google_Sans_Code" )
            .SetPath( "static" )
            .SetPath( "GoogleSansCode-Light.ttf" )
            .Build() };

        if ( consoleFontPath.IsFile() ) {
            AddFont( consoleFontPath );
        }

        try {
            InitImplementation();
        } catch ( ... ) {
            mImplementation.reset();
            mImGuiFonts.clear();
            ImPlot::DestroyContext();
            ImGui::DestroyContext();
            throw;
        }

        mIsInitialized = true;
    }

    auto ImGuiService::AddInterfaceFont( const filesystem::Path& path, core::u32 faceIndex ) -> ImFont* {
        if ( !path.IsFile() ) {
            return nullptr;
        }

        auto* atlas{ ImGui::GetIO().Fonts };

        // Keep the editor's icon glyphs when a system font includes its own private-use characters.
        // https://github.com/ocornut/imgui/blob/v1.92.9b/docs/FONTS.md
        static constexpr ImWchar iconCodePoints[]{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };
        ImFontConfig config{};
        config.FontNo = faceIndex;
        config.GlyphExcludeRanges = iconCodePoints;

        auto* font{ atlas->AddFontFromFileTTF( path.GetC_Str(), kFontBaseSize, &config, atlas->GetGlyphRangesDefault() ) };
        if ( !font ) {
            return nullptr;
        }

        // Preserve the existing Japanese fallback for every interface face.
        const filesystem::Path notoSansPath{ filesystem::PathBuilder()
            .SetPath( mFontsRootDir.GetC_Str() )
            .SetPath( "Noto_Sans_JP" )
            .SetPath( "static" )
            .SetPath( "NotoSansJP-Light.ttf" )
            .Build() };

        ImFontConfig jpConfig{};
        jpConfig.MergeMode = true;
        jpConfig.DstFont = font;
        jpConfig.OversampleH = 2;
        jpConfig.OversampleV = 2;
        jpConfig.PixelSnapH = false;
        jpConfig.GlyphExcludeRanges = iconCodePoints;
        if ( notoSansPath.IsFile() ) {
            atlas->AddFontFromFileTTF( notoSansPath.GetC_Str(), kFontBaseSize, &jpConfig, atlas->GetGlyphRangesJapanese() );
        }

        const filesystem::Path fontPath{
            filesystem::PathBuilder()
                .SetPath( mImGuiFilesRootDir.GetC_Str() )
                .Build()
        };

        // Made static because ImGui does not extend lifetime
        static constexpr eastl::array<ImWchar, 3> iconRanges1{ ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        static const auto faRegular{
            filesystem::PathBuilder()
                .SetPath( fontPath.GetC_Str() )
                .SetPath( FONT_ICON_FILE_NAME_FAS )
                .Build()
        };

        AddIconFont( faRegular, iconRanges1, font );

        // See https://react-icons.github.io/react-icons/icons?name=md for icon previews
        static constexpr eastl::array<ImWchar, 3> iconRanges2{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };
        static const auto materialIconsRegular{
            filesystem::PathBuilder()
                .SetPath( fontPath.GetC_Str() )
                .SetPath( FONT_ICON_FILE_NAME_MD )
                .Build()
        };

        AddIconFont( materialIconsRegular, iconRanges2, font );

        static constexpr eastl::array<ImWchar, 3> iconRanges3{ ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };
        static const auto materialDesign{
            filesystem::PathBuilder()
                .SetPath( fontPath.GetC_Str() )
                .SetPath( FONT_ICON_FILE_NAME_MDI )
                .Build()
        };

        AddIconFont( materialDesign, iconRanges3, font );

        return font;
    }

    auto ImGuiService::AddFont( const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();

        if ( auto* font{ ImGui::GetIO().Fonts->AddFontFromFileTTF( path.GetC_Str(), kFontBaseSize ) } ) {
            mImGuiFonts.try_emplace( filesystem::Path{ path.GetAbsolute() }, font );
        }
    }

    auto ImGuiService::AddIconFont( const filesystem::Path& path, const eastl::array<ImWchar, 3>& iconRanges, ImFont* target ) -> void {
        MKT_BEGIN_PROFILER();
        ImFontConfig config{};
        config.MergeMode = true;
        config.DstFont = target;
        config.GlyphMinAdvanceX = 4.0f;
        config.PixelSnapH = true;
        config.GlyphOffset.y = 4.0f;
        config.GlyphOffset.x = 0.0f;
        config.OversampleH = config.OversampleV = 3.0f;
        config.SizePixels = 12.0f;

        constexpr core::f32 iconFontSize{ kFontBaseSize * 1.01f };
        if ( path.IsFile() ) {
            ImGui::GetIO().Fonts->AddFontFromFileTTF( path.GetC_Str(), iconFontSize, &config, iconRanges.data() );
        }
    }

    auto ImGuiService::SetImGuiBackGroundClearColor( const core::float4 &color ) -> void {
        MKT_BEGIN_PROFILER();
        mImplementation->SetClearColor( color );
    }

    auto ImGuiService::GetTextureID( renderer::rhi::TextureHandle texture ) -> ImTextureID {
        MKT_BEGIN_PROFILER();
        return GetTextureID( texture.GetPtr() );
    }

    auto ImGuiService::GetTextureID( const renderer::rhi::ITexture *texture ) -> ImTextureID {
        MKT_BEGIN_PROFILER();
        return mImplementation->ConstructImGuiTextureID( texture );
    }

    auto ImGuiService::GetBackend() -> ImGuiBackend * {
        MKT_BEGIN_PROFILER();
        return mImplementation.get();
    }

    auto ImGuiService::GetBackend() const -> const ImGuiBackend * {
        MKT_BEGIN_PROFILER();
        return mImplementation.get();
    }

    auto ImGuiService::PushFont( eastl::string_view str ) -> ImGuiScopedTextFont {
        MKT_BEGIN_PROFILER_NAMED();

        const filesystem::Path path{ filesystem::Path{ str }.GetAbsolute() };

        if ( const auto found{ mImGuiFonts.find( path ) }; found != mImGuiFonts.end() ) {
            // Inspector previews reuse their font every frame. Avoid filesystem
            // refreshes once the ImGui font has already been registered.
            return ImGuiScopedTextFont{ found->second };
        }

        const auto fontFile{ filesystem::FileService::Get()->LoadFile( path ) };

        if ( fontFile.IsEmpty() ) {
            MKT_CORE_LOGGER_WARN( "ImGuiService::PushFont - Failed to load font at {}", str );
            return ImGuiScopedTextFont{ ImGuiScopedTextFont::Invalid };
        }

        AddFont( fontFile->GetPath() );
        const auto found{ mImGuiFonts.find( path ) };
        return ImGuiScopedTextFont{ found == mImGuiFonts.end() ? nullptr : found->second };
    }


    auto ImGuiService::InitImplementation() -> void {
        MKT_BEGIN_PROFILER();
        ImGuiIO &io{ ImGui::GetIO() };

        // Load ini file (static because IniFilename is const char*)
        // and it will not extend iniFilePath lifetime
        static const filesystem::Path iniFilePath{
            filesystem::PathBuilder()
                .SetPath( mImGuiFilesRootDir.GetC_Str() )
                .SetPath( "imgui.ini" )
                .Build()
        };

        io.IniFilename = iniFilePath.GetC_Str();

        // Create implementation
        const ImGuiBackendCreateInfo imGuiBackendCreateInfo{
            .mWindow = mWindow,
            .mDevice = mDevice,
            .mApi = mBackendApi
        };
        mImplementation = ImGuiBackend::Create( imGuiBackendCreateInfo );

        // Initialize the implementation
        if ( mImplementation ) {
            mImplementation->Init();
        }
        else {
            MKT_CORE_LOGGER_ERROR( "Failed to initialize an ImGui backend!" );
        }
    }

    auto ImGuiBackend::Create( const ImGuiBackendCreateInfo &info ) -> eastl::unique_ptr<ImGuiBackend> {
        MKT_BEGIN_PROFILER();

        switch ( info.mApi ) {
            case renderer::rhi::GraphicsAPI::eOpenGL:
                return eastl::make_unique<ImGuiOpenGLBackend>( info );
            case renderer::rhi::GraphicsAPI::eVulkan:
                return eastl::make_unique<ImGuiVulkanBackend>( info );
#if defined( MIKOTO_PLATFORM_WINDOWS )
            case renderer::rhi::GraphicsAPI::eD3D12:
                return eastl::make_unique<ImGuiD3D12Backend>( info );
            case renderer::rhi::GraphicsAPI::eD3D11:
                return eastl::make_unique<ImGuiD3D11Backend>( info );
#endif
            default:;
        }

        return nullptr;
    }

    auto ImGuiService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down ImGuiService..." );

        mImplementation->Shutdown();
        mImplementation.reset();

        mPendingInterfaceFont.reset();
        mSelectedInterfaceFont = nullptr;
        mImGuiFonts.clear();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    auto ImGuiService::EndFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mImplementation->EndFrame();
    }

    auto ImGuiService::QueueLayoutRestore( eastl::string settings ) -> void {
        mPendingLayout = eastl::move( settings );
    }

    auto ImGuiService::QueueInterfaceFont( const filesystem::Path& path, core::u32 faceIndex ) -> void {
        mPendingInterfaceFont = InterfaceFontRequest{
            .mPath = path,
            .mFaceIndex = faceIndex
        };
    }

    auto ImGuiService::ApplyInterfaceFont() -> void {
        if ( !mPendingInterfaceFont ) {
            return;
        }

        const auto& request{ *mPendingInterfaceFont };
        auto* font{ request.mPath.IsEmpty() ? nullptr : AddInterfaceFont( request.mPath, request.mFaceIndex ) };
        auto& io{ ImGui::GetIO() };

        // Retire the previous selection so browsing system fonts cannot accumulate faces.
        if ( mSelectedInterfaceFont ) {
            io.Fonts->RemoveFont( mSelectedInterfaceFont );
        }

        io.FontDefault = font;
        mSelectedInterfaceFont = font;
        mPendingInterfaceFont.reset();
    }

    auto ImGuiService::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ApplyInterfaceFont();

        if ( !mPendingLayout.empty() ) {
            // Dock nodes may be referenced by active windows. Replace them only between frames.
            ImGui::ClearIniSettings();
            ImGui::LoadIniSettingsFromMemory( mPendingLayout.data(), mPendingLayout.size() );
            mPendingLayout.clear();
        }

        // Floating docking nodes update their tab bars during NewFrame.
        const ImGuiScopedStyleVar tabPadding{ ImGuiStyleVar_FramePadding, kWindowTabPadding };
        mImplementation->BeginFrame();
    }

    auto ImGuiService::GetFinalComposition() const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mImplementation->GetFinalComposition();
    }

    ImGuiBackend::ImGuiBackend( const ImGuiBackendCreateInfo &createInfo )
        : mWindow{ createInfo.mWindow }, mDevice{ createInfo.mDevice } {
    }

    auto ImGuiBackend::GetSDLWindow() const -> SDL_Window* {
        const auto native{ mWindow ? mWindow->GetNativeWindow() : eastl::any{} };
        const auto* window{ eastl::any_cast<SDL_Window*>( &native ) };

        if ( !window || !*window ) {
            MKT_THROW_RUNTIME_ERROR( "The ImGui SDL backend requires an SDL window." );
        }

        return *window;
    }

    auto ImGuiBackend::SetClearColor( const core::float4 &color ) -> void {
        MKT_BEGIN_PROFILER();
        mClearColor = color;
    }

    auto ImGuiBackend::SetResolution( renderer::rhi::RenderResolution reso ) -> void {
        MKT_BEGIN_PROFILER();
        mResolution = reso;
    }

    auto ImGuiBackend::GetResolution() const -> renderer::rhi::RenderResolution {
        MKT_BEGIN_PROFILER();
        return mResolution;
    }
}
