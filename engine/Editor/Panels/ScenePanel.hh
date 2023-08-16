/**
 * ScenePanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_HH
#define MIKOTO_SCENE_PANEL_HH

// C++ Standard Library

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/OpenGL/OpenGLRenderer.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Editor/Panels/Panel.hh>
#include <Editor/Panels/PanelData.hh>

// TODO: specialize panels for eahc graphics API using CRTP
namespace Mikoto {
    class ScenePanel : public Panel {
    public:
        explicit ScenePanel() = default;
        explicit ScenePanel(const std::shared_ptr<ScenePanelData> &data, const Path_T& iconPath = {});

        ScenePanel(const ScenePanel& other) = default;
        ScenePanel(ScenePanel&& other) = default;

        auto operator=(const ScenePanel& other) -> ScenePanel& = default;
        auto operator=(ScenePanel&& other) -> ScenePanel& = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) ->  void override;
        auto MakeVisible(bool value) ->  void override { m_Visible = value; }

        KT_NODISCARD auto IsHovered() const -> bool override { return m_Hovered; }
        KT_NODISCARD auto IsFocused() const -> bool override { return m_Focused; }
        KT_NODISCARD auto IsVisible() const -> bool override { return m_Visible; }
    private:
        bool m_Visible{};
        bool m_Hovered{};
        bool m_Focused{};

        std::shared_ptr<ScenePanelData> m_Data{};

    private:
        VkSampler m_ColorAttachmentSampler{};
        VkDescriptorSet m_DescriptorSet{};

        // TODO: avoid dynamic casts and easily access renderer specific  functionality
        VulkanRenderer* m_SceneRendererVk{};
        OpenGLRenderer* m_SceneRendererOGL{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
