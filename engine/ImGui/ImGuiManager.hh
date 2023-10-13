/**
 * ImGuiLayer.hh
 * Created by kate on 5/28/23.
 * */

#ifndef MIKOTO_IMGUI_LAYER_HH
#define MIKOTO_IMGUI_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Event.hh>
#include <Core/Layer.hh>
#include <ImGui/ImGuiUtils.hh>
#include <Platform/Window.hh>

namespace Mikoto {
    /**
     * ImGuiLayer interface for ImGui GUI elements
     * Uses OpenGL/Vulkan for rendering and GLFW for event handling
     * */
    class ImGuiManager {
    public:
        static auto Init(const std::shared_ptr<Window>& window) -> void;

        /**
         * Starts a new ImGui frame. Must call before submitting any ImGui item
         * */
        static auto BeginFrame() -> void;

        /**
         * Ends an ImGui frame. Call to submit ImGui items to be rendered
         * */
        static auto EndFrame() -> void;

        static auto GetFonts() -> std::vector<ImFont*>& { return s_Fonts; }

        /**
         * Destroys ImGui context
         * */
        static auto Shutdown() -> void;

    private:
        static auto AddIconFont(float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void;

        /**
         * Initializes the underlying implementation. Must call after OnAttach,
         * currently the implementation supports Vulkan and OpenGL, but both
         * of them work with GLFW for the window management and event handling
         * therefore the window native handle must be from GLFW. The behaviour is undefined
         * if window's native handle is not of GLFW
         * @param window window handle
         * */
        static auto InitImplementation(const std::shared_ptr<Window>& window) -> void;

    private:
        inline static std::vector<ImFont*> s_Fonts{};
        inline static std::unique_ptr<BackendImplementation> m_Implementation{ nullptr };
    };
}

#endif // MIKOTO_IMGUI_LAYER_HH
