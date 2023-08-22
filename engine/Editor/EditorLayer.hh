/**
 * EditorLayer.hh
 * Created by kate on 6/12/23.
 * */

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Layer.hh>
#include <Renderer/Buffers/FrameBuffer.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Camera/OrthographicCamera.hh>
#include <Renderer/Material/Shader.hh>
#include <Renderer/Material/Texture.hh>
#include <Renderer/Model.hh>
#include <Renderer/Camera/Camera.hh>
#include <Editor/Editor.hh>
#include <Editor/Panels/HierarchyPanel.hh>
#include <Editor/Panels/InspectorPanel.hh>
#include <Editor/Panels/ScenePanel.hh>
#include <Editor/Panels/SettingsPanel.hh>
#include <Editor/Panels/StatsPanel.hh>
#include <Editor/Panels/Panel.hh>
#include <Editor/Panels/PanelData.hh>
#include <Scene/Scene.hh>
#include <Scene/Entity.hh>

namespace Mikoto {

    class EditorLayer : public Layer {
    public:
        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;
        auto OnEvent(Event& event) -> void override;
        auto OnImGuiRender() -> void override;
    private:
        std::shared_ptr<SceneCamera> m_MainCamera{};

        // Panels
        std::shared_ptr<HierarchyPanel> m_HierarchyPanel{};
        std::shared_ptr<InspectorPanel> m_InspectorPanel{};
        std::shared_ptr<SettingsPanel> m_SettingsPanel{};
        std::shared_ptr<ScenePanel> m_ScenePanel{};
        std::shared_ptr<StatsPanel> m_StatsPanel{};

        // Panels data
        std::shared_ptr<SettingsPanelData> m_SettingsPanelInfo{};
        std::shared_ptr<ScenePanelData> m_ScenePanelInfo{};
        std::shared_ptr<StatsPanelData> m_StatsPanelInfo{};

        Model m_TestModel{};

    };

}

#endif // MIKOTO_EDITOR_LAYER_HH
