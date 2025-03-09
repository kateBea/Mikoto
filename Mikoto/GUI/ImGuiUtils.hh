/**
 * ImGuiUtils.hh
 * Created by kate on 9/17/23.
 * */

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

// C++ Standard Library
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#include <volk.h>

#include <Common/Common.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>
#include <Material/Texture/Texture2D.hh>
#include <any>
#include <glm/gtc/type_ptr.hpp>


namespace Mikoto::ImGuiUtils {

    class ImGuiScopedStyleVar {
    public:

        template<typename... Args>
        explicit ImGuiScopedStyleVar( Args&&... args ) {
            ImGui::PushStyleVar(std::forward<Args>(args)...);
        }

        ~ImGuiScopedStyleVar() {
            ImGui::PopStyleVar();
        }
    };

    MKT_NODISCARD inline auto PushImageButton(UInt64_T textureId, const VkDescriptorSet textureHandle, const ImVec2 size) -> bool {
        const ImTextureID icon{ reinterpret_cast<ImTextureID>( textureHandle ) };
        return ImGui::ImageButton( StringUtils::ToString( textureId ).c_str(), icon, size, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
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

        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.124f, 0.124f, 0.124f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.184f, 0.184f, 0.184f, 0.00f);
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

    inline auto HelpMarker( const std::string_view description, const std::string_view placeHolder = "(?)") -> void {
        ImGui::TextDisabled("%s", placeHolder.data());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(description.data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    inline auto CheckBox( const CStr_T label, bool& value) -> bool{
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };

        bool active{ ImGui::Checkbox( label, std::addressof( value ) ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto ToolTip( const std::string_view description) -> void {
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

        if (ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(description.data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    inline auto ToolTip( const std::function<void()>& func, const bool enable ) -> void {
        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if ( enable && ImGui::BeginTooltip() ) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );

            func();

            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    inline auto DragFloat4(const CStr_T label, const CStr_T format,  glm::vec4& vect, float speed, float minVal, float maxVal) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat4( label, value_ptr( vect ), speed, minVal, maxVal, format ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto ColorEdit4(const CStr_T label, glm::vec4& vect) -> bool {
        constexpr ImGuiColorEditFlags colorEditFlags{
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar
        };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::ColorEdit4( label, value_ptr( vect ), colorEditFlags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto Slider(const CStr_T label, float& value, const glm::vec2& bounds, std::string_view format = "%.2f") -> bool {
        constexpr ImGuiSliderFlags flags{ ImGuiSliderFlags_None };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        const bool active{ ImGui::SliderFloat( label, std::addressof( value ), bounds.x, bounds.y, format.data(), flags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto ButtonTextIcon(const CStr_T icon, ImVec2 size = { 0.0f, 0.0f }) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2  { 0.0f, 0.0f } };
        ImGuiScopedStyleVar framePadding{ ImGuiStyleVar_FramePadding, ImVec2  { 4.0f, 4.0f } };

        const bool active{ ImGui::Button(fmt::format("{}", icon).c_str()) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto TextArea(std::string& contents) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        constexpr  ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AllowTabInput };

        ImVec2 windowSize{ ImGui::GetWindowSize() };

        const bool active{ ImGui::InputTextMultiline("##TextInput", contents.data(), contents.size(), ImVec2(windowSize.x * 0.5f, ImGui::GetTextLineHeight() * 5), flags) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_TextInput );
        }

        return active;
    }

    inline auto CenteredText(const char* label, const float width, float height = 20.0f) -> void {
        // https://github.com/phicore/ImGuiStylingTricks/wiki/Custom-MessageBox#step-5-removed-title-bar-and-homemade-centered-text

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style{ g.Style };

        const ImVec2 textSize{ width, height };
        const ImGuiWindow* window{ ImGui::GetCurrentWindow() };

        const ImVec2 labelSize{ ImGui::CalcTextSize(label, nullptr, true) };

        const ImVec2 minCursorPos{ window->DC.CursorPos };
        const ImVec2 itemSize{ ImGui::CalcItemSize(textSize, labelSize.x + style.FramePadding.x * 2.0f, labelSize.y + style.FramePadding.y * 2.0f) };

        const ImVec2 maxCursorPos{ ImVec2(minCursorPos.x + itemSize.x, minCursorPos.y + itemSize.y) };
        const ImRect alignment{ minCursorPos, maxCursorPos };

        ImGui::ItemSize(itemSize, style.FramePadding.y);

        const ImVec2 posMin{ ImVec2( alignment.Min.x + style.FramePadding.x, alignment.Min.y + style.FramePadding.y ) };
        const ImVec2 posMax{ ImVec2( alignment.Max.x - style.FramePadding.x, alignment.Max.y - style.FramePadding.y ) };

        ImGui::RenderTextClipped(posMin, posMax, label, nullptr, &labelSize, style.ButtonTextAlign, &alignment);
    }
}

#endif // MIKOTO_IMGUI_UTILS_HH
