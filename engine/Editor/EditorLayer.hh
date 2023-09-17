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
#include <Editor/Editor.hh>
#include <Renderer/Model.hh>
#include <Renderer/Camera/Camera.hh>
#include <Renderer/Buffers/FrameBuffer.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Camera/OrthographicCamera.hh>
#include <Renderer/Material/Shader.hh>
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
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        auto InitializePanels() -> void;

        /**
         * Properly initializes and adds two sprites to the scene
         * @deprecated Now we are able to add game objects within the editor
         * */
        auto AddSceneTestEntities() -> void;

        /**
         * Properly initializes and adds two cameras to the scene. One of them
         * is the editor camera which will be active when we are editing the scene.
         * The other is the runtime camera which is active when we are in the "play mode"
         * This last camera should generally be added at runtime since the user
         * is responsible of having a view to the world when it is playing, if
         * there's no runtime camera we see nothing when the scene is playing
         * */
        auto InitializeSceneCameras() -> void;

    private:
        /*************************************************************
        * DATA MEMBERS
        * ***********************************************************/
        std::shared_ptr<SceneCamera> m_RuntimeCamera{};
        std::shared_ptr<EditorCamera> m_EditorCamera{};

        // Panels
        std::shared_ptr<HierarchyPanel> m_HierarchyPanel{};
        std::shared_ptr<InspectorPanel> m_InspectorPanel{};
        std::shared_ptr<SettingsPanel> m_SettingsPanel{};
        std::shared_ptr<ScenePanel> m_ScenePanel{};
        std::shared_ptr<StatsPanel> m_StatsPanel{};
    };

}

#endif // MIKOTO_EDITOR_LAYER_HH
