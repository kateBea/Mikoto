/**
 * Created by kate on 9/17/23.
 * */

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

// C++ Standard Library
#include <any>

#include "imgui.h"

#include "Common/Common.hh"
#include "Common/Types.hh"
namespace Mikoto {
    struct ColorValues {
        Int32_T Red{};
        Int32_T Green{};
        Int32_T Blue{};
        Int32_T Alpha{};
    };

    /**
     * This class encapsulates backend implementation specific details. ImGui is a graphics API
     * agnostic GUI library and provides several implementations, each for a specific graphics backend.
     * This class serves as a general abstraction over the currently active backend in use in the application
     * that will also be used with ImGui
     * */
    class BackendImplementation {
    public:
        virtual auto Init(std::any windowHandle) -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual ~BackendImplementation() = default;
    };

    inline auto DrawColoredText(std::string_view text, ColorValues color) -> void {
        // TODO:
    }

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
