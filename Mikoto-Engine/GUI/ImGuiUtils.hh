/**
 * ImGuiUtils.hh
 * Created by kate on 9/17/23.
 * */

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

// C++ Standard Library
#include <any>

#include <imgui.h>

#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto::ImGuiUtils {
    inline auto PushImageButton(const Texture2D *texture, ImVec2 size, ImVec2 uv1 = ImVec2{ 0, 1 }, ImVec2 uv2 = ImVec2{ 1, 0 }) -> void {
        ImTextureID icon{};

        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                icon = reinterpret_cast<ImTextureID>(std::any_cast<UInt32_T>(texture->GetImGuiTextureHandle()));
                break;
            case GraphicsAPI::VULKAN_API:
                icon = (ImTextureID)std::any_cast<VkDescriptorSet>(texture->GetImGuiTextureHandle());
                break;
        }

        ImGui::ImageButton(icon, size, uv1, uv2);
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
        virtual auto Init(std::any windowHandle) -> void = 0;
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
