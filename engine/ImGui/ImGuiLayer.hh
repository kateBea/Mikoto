/**
 * ImGuiLayer.hh
 * Created by kate on 5/28/23.
 * */

#ifndef MIKOTO_IMGUI_LAYER_HH
#define MIKOTO_IMGUI_LAYER_HH

// Third-Party Libraries
#include "GLFW/glfw3.h"
#include "imgui.h"

// Project Headers
#include "Core/Layer.hh"
#include "Core/Events/AppEvents.hh"
#include "Core/Events/Event.hh"
#include "Core/Events/KeyEvents.hh"
#include "Core/Events/MouseEvents.hh"
#include "Renderer/Vulkan/VulkanCommandPool.hh"

namespace Mikoto {
    /**
     * ImGuiLayer interface for ImGui GUI elements
     * Uses OpenGL/Vulkan for rendering and GLFW for event handling
     * */
    class ImGuiLayer : public Layer {
    public:
        explicit ImGuiLayer() noexcept;
        ~ImGuiLayer() override = default;

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;
        auto OnEvent(Event& event) -> void override;
        auto OnImGuiRender() -> void override;

        auto SetBlockEvents(bool value) -> void { m_BlockEvents = value; }

        auto BeginFrame() -> void;
        auto EndFrame() -> void;
    private:
        /*************************************************************
        * STRUCTURES
        * ***********************************************************/
    private:
        /*************************************************************
        * OPENGL
        * ***********************************************************/
        auto InitImGuiForOpenGL(GLFWwindow* window) -> void;
        auto BeginImGuiFrameForOpenGL() -> void;
        auto EndImGuiFrameForOpenGL() -> void;

        /*************************************************************
        * VULKAN
        * ***********************************************************/
        auto InitImGuiForVulkan(GLFWwindow *window) -> void;
        auto BeginImGuiFrameForVulkan() -> void;
        auto EndImGuiFrameForVulkan() -> void;

        auto CreateImGuiRenderPass() -> void;
        auto CreateImGuiCommandPool() -> void;
        auto CreateImGuiCommandBuffers() -> void;

        static auto AcquireNextSwapChainImage(UInt32_T& imageIndex) -> VkResult;
        auto RecordImGuiCommandBuffers(UInt32_T imageIndex) -> void;
        auto SubmitImGuiCommandBuffers(UInt32_T& imageIndex) -> VkResult;
    private:
        // Do not propagate events to this layer
        bool m_BlockEvents{ false };

        bool m_UseOpenGL{ false };
        bool m_UseVulkan{ false };

        /*************************************************************
        * VULKAN MEMBERS
        * ********************************************************+ */
        VkDescriptorPool m_ImGuiDescriptorPool{};
        VkRenderPass m_ImGuiRenderPass{};

        std::shared_ptr<VulkanCommandPool> m_CommandPool{};
        std::vector<VkCommandBuffer> m_ImGuiCommandBuffers{};
    };

}


#endif // MIKOTO_IMGUI_LAYER_HH
