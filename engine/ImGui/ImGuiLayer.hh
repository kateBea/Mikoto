/**
 * ImGuiLayer.hh
 * Created by kate on 5/28/23.
 * */

#ifndef MIKOTO_IMGUI_LAYER_HH
#define MIKOTO_IMGUI_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Layer.hh>
#include <ImGui/ImGuiUtils.hh>
#include <Core/Events/Event.hh>

namespace Mikoto {
    /**
     * ImGuiLayer interface for ImGui GUI elements
     * Uses OpenGL/Vulkan for rendering and GLFW for event handling
     * */
    class ImGuiLayer : public Layer {
    public:
        explicit ImGuiLayer() noexcept;

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;
        auto OnEvent(Event& event) -> void override;

        auto SetBlockEvents(bool value) -> void { m_BlockEvents = value; }

        auto BeginFrame() -> void;
        auto EndFrame() -> void;

        ~ImGuiLayer() override = default;

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/

    private:
        // Do not propagate events to this layer
        // Make this flag part of the base layer, this way layers can decide when they want to accept events and when not
        bool m_BlockEvents{ false };
        std::unique_ptr<BackendImplementation> m_Implementation{};
    };

}


#endif // MIKOTO_IMGUI_LAYER_HH
