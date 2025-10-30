//
// Created by kate on 10/13/25.
//

#ifndef GRAPHICSLAYER_HH
#define GRAPHICSLAYER_HH
#include <string_view>

#include <Assets/AssetsService.hh>
#include <Assets/Model.hh>
#include <Core/LayerStack.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/SceneRenderer.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>
#include <Platform/Window.hh>

namespace Mikoto {

    class GraphicsLayer final : public ILayer {
    public:
        explicit GraphicsLayer( std::string_view name, const Window* window );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float deltaTime ) -> void override;

        auto OnEvent(Event &event) -> void override;

    private:
        auto LoadModels() -> void;
        auto SetupScene() -> void;
        auto SetupCamera() -> void;
        auto SetupRenderer() -> void;

        auto UpdateListener() -> void;

        auto UpdateCamera( float timeStep ) -> void;
        auto DrawViewport() const -> void;
        auto DisplayCameraDebugInfo() const -> void;

    private:
        Entity* m_Listener{ nullptr };

        Unique<Scene> m_MainScene{};
        Unique<SceneCamera> m_SceneCamera{};
        Unique<SceneRenderer> m_Renderer{};

        const Window* m_Window{};

        ModelHandle m_ModelSingleMesh{};
        ModelHandle m_ModelMultipleMeshes{};
    };
}



#endif //GRAPHICSLAYER_HH
