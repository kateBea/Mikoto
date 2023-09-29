//
// Created by kate on 9/14/23.
//

#include <imgui.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>

#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <ImGui/ImGuiOpenGLBackend.hh>

namespace Mikoto {
    auto ImGuiOpenGLBackend::Init(std::any windowHandle) -> void {
        // At the moment we are using Vulkan with GLFW windows
        GLFWwindow* window{ nullptr };

        try {
            window = std::any_cast<GLFWwindow*>(windowHandle);
        }
        catch (std::bad_any_cast& e) {
            MKT_CORE_LOGGER_ERROR("Failed on any cast std::any windowHandle to GLFWwindow* for ImGuiOpenGLBackend::Init");
            return;
        }

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        const std::string openglVersion{ fmt::format("#version {}{}0", MKT_OPENGL_VERSION_MAJOR, MKT_OPENGL_VERSION_MINOR) };
        ImGui_ImplOpenGL3_Init(openglVersion.c_str());
    }

    auto ImGuiOpenGLBackend::ShutDown() -> void {
        ImGui_ImplOpenGL3_Shutdown();
    }

    auto ImGuiOpenGLBackend::BeginFrame() -> void {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiOpenGLBackend::EndFrame() -> void {
        ImGuiIO& io{ ImGui::GetIO() };
        Window& window{ Application::Get().GetMainWindow() };
        io.DisplaySize = ImVec2{ (float) window.GetWidth(), (float) window.GetHeight() };

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow *backupCurrentContext{ glfwGetCurrentContext()};
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            // Implies we are using OpenGL with GLFW
            glfwMakeContextCurrent(backupCurrentContext);
        }
    }
}