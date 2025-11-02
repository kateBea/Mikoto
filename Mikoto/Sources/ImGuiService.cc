//
// Created by zanet on 1/26/2025.
//

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include <tracy/Tracy.hpp>

    // Project Headers
#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>
#include <imgui_impl_vulkan.h>

#include <Common/Common.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/ImGuiVulkanBackend.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>
#include <Renderer/RenderService.hh>

namespace Mikoto {

    ImGuiService::ImGuiService( const ImGuiServiceDescription &options )
        : m_Device{ options.Device },
         m_ImGuiFilesRootDir{ PathBuilder()
        .WithPath( "Resources" )
        .WithPath( "ImGui" )
        .Build().string() },
        m_BackendApi{ options.BackendApi },
        m_Window{ options.TargetWindow }
    {}

    auto ImGuiService::Init() -> void {
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
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Configure ImGui Style
        ImGui::StyleColorsDark();
        ImGuiUtils::ThemeDarkModeDefault();

        // General look and feel

        style.FrameRounding  = 6.0f;  // Buttons, sliders, etc.
        style.GrabRounding   = 6.0f;  // Grabs on sliders
        style.WindowRounding = 8.0f;  // Top-level windows
        style.ChildRounding  = 8.0f;  // Child windows
        style.PopupRounding  = 8.0f;  // Popups
        style.ScrollbarRounding = 8.0f;

        constexpr float iconFontSize{ FONT_BASE_SIZE * 1.1f }; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly };

        std::string path{ PathBuilder()
                .WithPath( m_ImGuiFilesRootDir )
                .WithPath( "JetBrainsMono/fonts/ttf/" )
                .WithPath( "JetBrainsMonoNL-Thin.ttf" )
                .Build().string() };

        AddFont(FONT_BASE_SIZE, path);

        const std::string fontPath{
            PathBuilder()
            .WithPath( m_ImGuiFilesRootDir )
            .Build().string() };

        static constexpr std::array<ImWchar, 3> iconRanges1{ ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        static const auto faRegular{ PathBuilder().WithPath( fontPath ).WithPath( FONT_ICON_FILE_NAME_FAS ).Build() };
        AddIconFont(iconFontSize, faRegular.string(), iconRanges1);

        // See https://react-icons.github.io/react-icons/icons?name=md for icon previews
        static constexpr std::array<ImWchar, 3> iconRanges2{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };
        static const auto materialIconsRegular{ PathBuilder().WithPath( fontPath ).WithPath( FONT_ICON_FILE_NAME_MD ).Build() };
        AddIconFont(iconFontSize, materialIconsRegular.string(), iconRanges2);

        static constexpr std::array<ImWchar, 3> iconRanges3{ ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };
        static const auto materialDesign{ PathBuilder().WithPath( fontPath ).WithPath( FONT_ICON_FILE_NAME_MDI ).Build() };
        AddIconFont(iconFontSize, materialDesign.string(), iconRanges3);

        InitImplementation();

        m_IsInitialized = true;
    }

    auto ImGuiService::AddFont( float fontSize, const std::string &path, const ImFontConfig* config, const std::array<ImWchar, 3>* iconRanges ) -> void {
        ImGuiIO &io{ ImGui::GetIO() };

        auto result{ io.Fonts->AddFontFromFileTTF(path.c_str(), fontSize, config, iconRanges->data() ) };

        // ImGui pushes new fonts into the io.Fonts array when we add them using MergeMode == true
        // see imgui_draw_.cpp ImFont* ImFontAtlas::AddFont(const ImFontConfig* font_cfg_in)
        // Also first font cannot have MergeMode == true
        if (m_ImGuiFonts.empty() || result && (config == nullptr || !config->MergeMode)) {
            m_ImGuiFonts.try_emplace( path, static_cast<Int8>( m_ImGuiFonts.size() ) );
        }
    }

    auto ImGuiService::AddIconFont( const float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges ) -> void {

        ImFontConfig config{};
        config.MergeMode = true;
        config.GlyphMinAdvanceX = 4.0f;
        config.PixelSnapH = true;
        config.GlyphOffset.y = 4.0f;
        config.GlyphOffset.x = 0.0f;
        config.OversampleH = config.OversampleV = 3.0f;
        config.SizePixels = 12.0f;

        AddFont(fontSize, path, std::addressof( config ), std::addressof( iconRanges ));
    }

    auto ImGuiService::SetImGuiBackGroundClearColor( const Vec4F &color ) -> void {
        m_Implementation->SetClearColor( color );
    }

    auto ImGuiService::GetTextureID( TextureHandle texture ) -> ImTextureID {
        return GetTextureID(texture.GetRaw());
    }

    auto ImGuiService::GetTextureID( const Texture *texture ) -> ImTextureID {
        return m_Implementation->ConstructImGuiTextureID( texture );
    }

    auto ImGuiService::GetBackend() -> ImGuiBackend * {
        return m_Implementation.get();
    }

    auto ImGuiService::GetBackend() const -> const ImGuiBackend * {
        return m_Implementation.get();
    }

    auto ImGuiService::PushFont( std::string_view str ) -> ImGuiUtils::ImGuiScopedTextFont {
        MKT_BEGIN_PROFILER_NAMED();

        auto it{ m_ImGuiFonts.find( std::string( str ) ) };
        if ( it == m_ImGuiFonts.end() ) {
            const File* fontFile{ FileService::Get()->LoadFile( Path{ str } ) };

            if (fontFile == nullptr) {
                MKT_CORE_LOGGER_WARN( "ImGuiService::PushFont - Failed to load font at {}", str );
                return ImGuiUtils::ImGuiScopedTextFont( ImGuiUtils::ImGuiScopedTextFont::Invalid );
            }

            AddFont( FONT_BASE_SIZE, fontFile->GetPath().c_str() );
        }

        return ImGuiUtils::ImGuiScopedTextFont{ m_ImGuiFonts.at( std::string( str ) ) };
    }


    auto ImGuiService::InitImplementation() -> void {
        ImGuiIO &io{ ImGui::GetIO() };

        // Load ini file (static because IniFilename is const char*)
        // it will not extend iniFilePath lifetime
        static const std::string iniFilePath{
            PathBuilder()
                    .WithPath( m_ImGuiFilesRootDir )
                    .WithPath( "imgui.ini" )
                    .Build()
                    .string()
        };

        io.IniFilename = iniFilePath.c_str();

        // Create implementation
        const ImGuiBackendCreateInfo imGuiVulkanBackendCreateInfo{
            .Handle{ m_Window },
            .API{ m_BackendApi },
            .Device{ m_Device },
        };

        m_Implementation = ImGuiBackend::Create( imGuiVulkanBackendCreateInfo );

        // Initialize the implementation
        if ( m_Implementation ) {
            m_Implementation->Init();
        } else {
            MKT_CORE_LOGGER_ERROR( "Failed to initialize an ImGui backend!" );
        }
    }

    auto ImGuiBackend::Create(const ImGuiBackendCreateInfo& info) -> Unique<ImGuiBackend> {
        switch (info.API) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<ImGuiVulkanBackend>(info);
            default:;
        }

        return nullptr;
    }

    auto ImGuiService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down ImGuiService..." );

        m_Implementation->Shutdown();
        m_Implementation = nullptr;

        ImGui::DestroyContext();
    }

    auto ImGuiService::EndFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Implementation->EndFrame();
    }

    auto ImGuiService::PrepareFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Implementation->BeginFrame();
    }

}// namespace Mikoto