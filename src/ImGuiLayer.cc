/**
* ImGuiLayer.cc
* Created by kate on 5/28/23.
* */

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include <volk.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

// Project Headers
#include <Utility/Common.hh>
#include "ImGui/ImGuiLayer.hh"
#include <Core/Application.hh>
#include <Core/Logger.hh>
#include <Editor/Editor.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <ImGui/ImGuiOpenGLBackend.hh>
#include <ImGui/ImGuiVulkanBackend.hh>

namespace Mikoto {
    ImGuiLayer::ImGuiLayer() noexcept
        :   Layer{ "ImGuiLayer" } {}

    auto ImGuiLayer::OnAttach() -> void {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport / Platform Windows

        // When viewports are enabled, we tweak WindowRounding/WindowBg so platform windows can
        // look identical to regular ones.
        ImGuiStyle &style{ImGui::GetStyle()};
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Configure ImGui Style
        ImGui::StyleColorsDark();
        Editor::ThemeDarkModeDefault();

        // Load fonts
        io.Fonts->AddFontFromFileTTF("../assets/Fonts/Open_Sans/static/OpenSans-Bold.ttf", 17.5);
        io.FontDefault = io.Fonts->AddFontFromFileTTF("../assets/Fonts/Open_Sans/OpenSans-VariableFont_wdth,wght.ttf", 17.5);

        // Load ini file
        io.IniFilename = "../assets/imgui/imgui.ini";

        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                m_Implementation = std::make_unique<ImGuiOpenGLBackend>();
                break;
            case GraphicsAPI::VULKAN_API:
                m_Implementation = std::make_unique<ImGuiVulkanBackend>();
                break;
        }

        if (m_Implementation) {
            m_Implementation->Init(Application::Get().GetMainWindowPtr()->GetNativeWindow());
        }
        else {
            MKT_CORE_LOGGER_ERROR("Failed to initialize an ImGui backend!");
        }
    }

    auto ImGuiLayer::OnDetach() -> void {
        m_Implementation->ShutDown();

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    auto ImGuiLayer::OnUpdate(double ts) -> void {

    }

    auto ImGuiLayer::EndFrame() -> void {
        m_Implementation->EndFrame();
    }

    auto ImGuiLayer::OnEvent(Event& event) -> void {
        if (m_BlockEvents) {
            ImGuiIO &io{ImGui::GetIO()};
            // To be revised: ImGuiLayer would not propagate mouse events and key events when and ImGui item captures them
            event.SetHandled(event.IsInCategory(MOUSE_EVENT_CATEGORY) && io.WantCaptureMouse);
            event.SetHandled(event.IsInCategory(KEY_EVENT_CATEGORY) && io.WantCaptureKeyboard);
        }
    }

    auto ImGuiLayer::BeginFrame() -> void {
        m_Implementation->BeginFrame();
    }
}