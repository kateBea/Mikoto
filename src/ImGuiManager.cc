/**
* ImGuiLayer.cc
* Created by kate on 5/28/23.
* */

// Third-Party Libraries
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Editor/Editor.hh>
#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiOpenGLBackend.hh>
#include <ImGui/ImGuiVulkanBackend.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <ImGui/ImGuiManager.hh>

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
        Editor::ThemeDarkModeDefault();

        const float baseFontSize{ 17.5f };

        // Load fonts
        io.FontDefault = io.Fonts->AddFontFromFileTTF("../assets/Fonts/Open_Sans/OpenSans-VariableFont.ttf", baseFontSize);
        // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
        const float iconFontSize{ baseFontSize * 2.0f/3.0f };

        ImFontConfig config{};
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.GlyphMinAdvanceX = iconFontSize;

        // TODO: has to be static
        static const std::array<ImWchar, 3> iconRanges1{ ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        static const std::array<ImWchar, 3> iconRanges2{ ICON_MIN_MD, ICON_MAX_16_MD, 0 };

        io.Fonts->AddFontFromFileTTF("../assets/Fonts/fa-regular-400.ttf", iconFontSize, &config, iconRanges1.data());
        io.Fonts->AddFontFromFileTTF("../assets/Fonts/MaterialIcons-Regular.ttf", iconFontSize, &config, iconRanges2.data());

        InitImplementation(window);
    }

    auto ImGuiManager::InitImplementation(const std::shared_ptr<Window>& window) -> void {
        MKT_CORE_LOGGER_INFO("Initializing ImGui implementation");

        ImGuiIO& io{ ImGui::GetIO() };

        // Load ini file
        io.IniFilename = "../assets/imgui/imgui.ini";

        // Create implementation
        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                m_Implementation = std::make_unique<ImGuiOpenGLBackend>();
                break;
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