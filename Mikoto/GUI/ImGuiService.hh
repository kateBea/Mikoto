//
// Created by zanet on 1/26/2025.
//

#ifndef GUISYSTEM_HH
#define GUISYSTEM_HH

#include <deque>
#include <functional>

#include <imgui.h>

#include <Common/Service.hh>
#include <GUI/ImGuiBackend.hh>
#include <Renderer/RendererBackend.hh>

namespace Mikoto {
    struct ImGuiServiceDescription {
        Path_T ImGuiFiles{};
        GraphicsAPI BackendApi{ GraphicsAPI::VULKAN_API };
        Window* TargetWindow{ nullptr };
    };

    class ImGuiService final : public IService<ImGuiService> {
    public:

        explicit ImGuiService(const ImGuiServiceDescription& options);

        ~ImGuiService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EndFrame() const -> void;
        auto PrepareFrame() const -> void;

        auto GetFonts() -> std::vector<ImFont*>& { return m_Fonts; }


    private:

        auto AddIconFont(float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void;
        auto InitImplementation() -> void;

    private:
        std::string m_ImGuiFilesRootDir{};
        GraphicsAPI m_BackendApi{ GraphicsAPI::INVALID_API };

        Window* m_Window{ nullptr };

        std::vector<ImFont*> m_Fonts{};
        Scope_T<ImGuiBackend> m_Implementation{ nullptr };
    };

}


#endif //GUISYSTEM_HH
