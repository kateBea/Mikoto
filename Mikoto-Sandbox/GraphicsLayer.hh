//
// Created by kate on 10/13/25.
//

#ifndef GRAPHICSLAYER_HH
#define GRAPHICSLAYER_HH
#include <string_view>

#include <Assets/Model.hh>
#include <Assets/AssetsService.hh>
#include <Renderer/RenderService.hh>
#include <Core/LayerStack.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

namespace Mikoto {

    class GraphicsLayer final : public ILayer {
    public:
        explicit GraphicsLayer( std::string_view name );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float deltaTime ) -> void override;

    private:
        auto SetupScene() -> void;

    private:
        BufferHandle m_VertexBuffer{};
        BufferHandle m_StagingBuffer{};
        TextureHandle m_Texture{};

        Unique<Scene> m_MainScene{};
        Unique<SceneCamera> m_SceneCamera{};

        ModelHandle m_Model{};
    };
}



#endif //GRAPHICSLAYER_HH
