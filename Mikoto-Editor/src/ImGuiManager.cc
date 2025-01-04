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

#include <../../Mikoto/Common/Common.hh>
#include <../../Mikoto/Common/RenderingUtils.hh>
#include <../../Mikoto/Core/Logger.hh>
#include <GUI/ImGuiManager.hh>
#include <GUI/ImGuiVulkanBackend.hh>

namespace Mikoto {
    auto ImGuiManager::Init(const std::shared_ptr<Window>& window) -> void {
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

        constexpr float baseFontSize{ 16.5f };
        constexpr float iconFontSize{ 18.0f };
        const std::string fontPath{ "../Assets/Fonts/" };


        // NOTE: FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly


        // Font 0
        s_Fonts.emplace_back(io.Fonts->AddFontFromFileTTF("../Assets/Fonts/JetBrainsMono/fonts/ttf/JetBrainsMonoNL-Light.ttf", 22.0f));

        // Font 1
        s_Fonts.emplace_back(io.Fonts->AddFontFromFileTTF("../Assets/Fonts/JetBrainsMono/fonts/ttf/JetBrainsMonoNL-Light.ttf", 17.0f));

        // Font 2
        s_Fonts.emplace_back(io.FontDefault = io.Fonts->AddFontFromFileTTF("../Assets/Fonts/Inter/static/Inter-Regular.ttf", baseFontSize));

        // Font 3
        static const std::array<ImWchar, 3> iconRanges1{ ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        AddIconFont(iconFontSize, (fontPath + FONT_ICON_FILE_NAME_FAR), iconRanges1);

        // Font 4
        // See https://react-icons.github.io/react-icons/icons?name=md for icon previews
        static const std::array<ImWchar, 3> iconRanges2{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };
        AddIconFont(iconFontSize, (fontPath + FONT_ICON_FILE_NAME_MD), iconRanges2);

        // Font 5
        static const std::array<ImWchar, 3> iconRanges3{ ICON_MIN_MDI, ICON_MAX_16_MDI, 0 };
        AddIconFont(iconFontSize, (fontPath + FONT_ICON_FILE_NAME_MDI), iconRanges3);

        InitImplementation(window);
    }

    auto ImGuiManager::AddIconFont(float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void {
        auto& io{ ImGui::GetIO() };

        ImFontConfig config{};
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.GlyphOffset.y = 4.0f;
        config.GlyphOffset.x = 0.0f;
        config.OversampleH = config.OversampleV = 3.0f;
        config.GlyphMinAdvanceX = 4.0f;
        config.SizePixels = 12.0f;

        s_Fonts.emplace_back(io.Fonts->AddFontFromFileTTF(
                path.c_str(),
                fontSize,
                std::addressof(config),
                iconRanges.data()));
    }

    auto ImGuiManager::InitImplementation(const std::shared_ptr<Window>& window) -> void {
        MKT_CORE_LOGGER_INFO("Initializing ImGui implementation");

        ImGuiIO& io{ ImGui::GetIO() };

        // Load ini file
        io.IniFilename = "../Assets/imgui/imgui.ini";

        // Create implementation
        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API:
                m_Implementation = std::make_unique<ImGuiVulkanBackend>();
                break;
        }

        // Initialize the implementation
        if (m_Implementation) {
            m_Implementation->Init(window->GetNativeWindow());
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