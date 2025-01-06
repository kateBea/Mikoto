/**
 * ImGuiUtils.hh
 * Created by kate on 9/17/23.
 * */

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

// C++ Standard Library
#include <any>

#include <imgui.h>

#include <STL/Utility/Types.hh>
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>

#include <Renderer/Core/Renderer.hh>
#include <Material/Texture/Texture2D.hh>

namespace Mikoto::ImGuiUtils {
    inline auto PushImageButton(const Texture2D *texture, const ImVec2 size, const ImVec2 uv1 = ImVec2{ 0, 1 }, const ImVec2 uv2 = ImVec2{ 1, 0 }) -> void {
        ImTextureID icon{};

        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API:
                icon = reinterpret_cast<ImTextureID>( std::any_cast<VkDescriptorSet>( texture->GetImGuiTextureHandle() ) );
                break;
        }

        ImGui::ImageButton( std::to_string( texture->GetID().Get() ).c_str(), icon, size, uv1, uv2);
    }


    inline auto ThemeDarkModeAlt() -> void {
        // Setup Dear ImGui style
        ImGuiStyle& style = ImGui::GetStyle();

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

    inline auto ThemeDarkModeDefault() -> void {
        // Setup Dear ImGui style
        ImGuiStyle &style = ImGui::GetStyle();

        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        style.Colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);

        style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

        style.Colors[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);


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
}

namespace Mikoto {
    /**
     * This class encapsulates backend implementation specific details. ImGui is a graphics API
     * agnostic GUI library and provides several implementations, each for a specific graphics backend.
     * This class serves as a general abstraction over the currently active backend in use in the application
     * that will also be used with ImGui
     * */
    class BackendImplementation {
    public:
        virtual auto Init(std::any windowHandle) -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual ~BackendImplementation() = default;
    };

    inline auto HelpMarker(std::string_view description, std::string_view placeHolder = "(?)") -> void {
        ImGui::TextDisabled("%s", placeHolder.data());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(description.data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}

#endif // MIKOTO_IMGUI_UTILS_HH
