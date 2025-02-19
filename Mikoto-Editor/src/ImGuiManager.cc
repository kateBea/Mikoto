/**
* ImGuiLayer.cc
* Created by kate on 5/28/23.
* */

// Third-Party Libraries
#include <../../Third-Party/imgui/backends/imgui_impl_glfw.h>
#include <../../Third-Party/imgui/imgui.h>

// Project Headers
#include <GUI/IconsFontAwesome5.h>
#include <GUI/IconsMaterialDesign.h>
#include <GUI/IconsMaterialDesignIcons.h>

#include <Common/Common.hh>
#include <Core/Engine.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/RenderSystem.hh>
#include <GUI/ImGuiManager.hh>
#include <GUI/ImGuiVulkanBackend.hh>
#include <Library/Filesystem/PathBuilder.hh>

namespace Mikoto {
    auto ImGuiManager::Init(const Window* window) -> void {
        MKT_CORE_LOGGER_INFO("Initializing ImGui manager");

        // Setup Dear ImGui context
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

        io.Fonts->AddFontDefault();
        constexpr float baseFontSize{ 18.0f };
        constexpr float iconFontSize{ 18.0f };

        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        const std::string fontPath{
            PathBuilder()
            .WithPath( fileSystem.GetFontsRootPath().string() )
            .Build().string() };


        // NOTE: FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

        // // Font 0
        s_Fonts.emplace_back(io.FontDefault = io.Fonts->AddFontFromFileTTF(PathBuilder()
                                 .WithPath( fileSystem.GetFontsRootPath().string() )
                                 .WithPath( "Inter" )
                                 .WithPath( "static" )
                                 .WithPath( "Inter-Regular.ttf" )
                                 .Build().string().c_str(), baseFontSize));


        // Font 3
        static constexpr std::array<ImWchar, 3> iconRanges1{ ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        static const auto faRegular{ PathBuilder().WithPath( fontPath ).WithPath( FONT_ICON_FILE_NAME_FAS ).Build() };
        AddIconFont(iconFontSize, faRegular.string(), iconRanges1);

        // Font 4
        // See https://react-icons.github.io/react-icons/icons?name=md for icon previews
        static constexpr std::array<ImWchar, 3> iconRanges2{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };
        static const auto materialIconsRegular{ PathBuilder().WithPath( fontPath ).WithPath( FONT_ICON_FILE_NAME_MD ).Build() };
        AddIconFont(iconFontSize, materialIconsRegular.string(), iconRanges2);

        // Font 5
        static constexpr std::array<ImWchar, 3> iconRanges3{ ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };
        static const auto materialDesign{ PathBuilder().WithPath( fontPath ).WithPath( FONT_ICON_FILE_NAME_MDI ).Build() };
        AddIconFont(iconFontSize, materialDesign.string(), iconRanges3);

        InitImplementation(window);
    }

    auto ImGuiManager::AddIconFont( const float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void {
        const auto& io{ ImGui::GetIO() };

        ImFontConfig config{};
        config.MergeMode = true;
        config.GlyphMinAdvanceX = 4.0f;
        config.PixelSnapH = true;
        config.GlyphOffset.y = 4.0f;
        config.GlyphOffset.x = 0.0f;
        config.OversampleH = config.OversampleV = 3.0f;
        config.SizePixels = 12.0f;

        auto font{ io.Fonts->AddFontFromFileTTF(
                path.c_str(),
                fontSize,
                std::addressof(config),
                iconRanges.data()) };

        s_Fonts.emplace_back(font);
    }

    auto ImGuiManager::InitImplementation(const Window* window) -> void {
        MKT_CORE_LOGGER_INFO("Initializing ImGui implementation");

        ImGuiIO& io{ ImGui::GetIO() };

        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        // Load ini file (static because IniFilename is const char*)
        // it will not extend iniFilePath lifetime
        static const auto iniFilePath{
            PathBuilder()
            .WithPath( fileSystem.GetImGuiDir().string() )
            .WithPath( "imgui.ini" )
            .Build().string()
        };

        io.IniFilename = iniFilePath.c_str();

        RenderSystem& renderSystem{ Engine::GetSystem<RenderSystem>() };

        // Create implementation
        ImGuiBackendCreateInfo imGuiVulkanBackendCreateInfo{
            .Handle{ window },
            .API{ renderSystem.GetDefaultApi() }
        };

        m_Implementation = CreateScope<ImGuiVulkanBackend>(imGuiVulkanBackendCreateInfo);

        // Initialize the implementation
        if (m_Implementation) {
            m_Implementation->Init();
        }
        else {
            MKT_CORE_LOGGER_ERROR("Failed to initialize an ImGui backend!");
        }
    }

    auto ImGuiManager::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting down ImGui");

        m_Implementation->Shutdown();

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    auto ImGuiManager::EndFrame() -> void {
        m_Implementation->EndFrame();
    }

    auto ImGuiManager::BeginFrame() -> void {
        m_Implementation->BeginFrame();
    }
}