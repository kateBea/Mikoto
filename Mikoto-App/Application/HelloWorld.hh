//
// Created by kate on 11/22/25.
//

#ifndef MIKOTO_HELLOWORLD_HH
#define MIKOTO_HELLOWORLD_HH

#include <../../Mikoto/Renderer/Core/SceneRenderer.hh>
#include <Common/Application.hh>
#include <Core/EventService.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window.hh>
#include <Scene/Scene.hh>

namespace MikotoApp {
    using namespace Mikoto;

    class HelloWorld final : public Application, public Subscriber {
    public:

        auto Run(Int32 argc, char** argv) -> Int32 override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

        auto SetWindow(Window* window) -> void;

    private:
        auto SetupRenderer(double timeStep ) const -> void;
        auto SetupCamera(double timeStep ) const -> void;

        Window* m_Window{};

        Unique<Scene> m_ActiveScene{};
        Unique<SceneCamera> m_Camera{};
        Unique<SceneRenderer> m_SceneRenderer{};

    };
}


#endif//MIKOTO_HELLOWORLD_HH
