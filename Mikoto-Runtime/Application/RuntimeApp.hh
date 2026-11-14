#ifndef MIKOTO_RUNTIME_APPLICATION_HH
#define MIKOTO_RUNTIME_APPLICATION_HH

#include <EASTL/unique_ptr.h>

#include <Core/Engine.hh>
#include <Core/Application.hh>

#include <Platform/EventSystem.hh>

#include <Layers/RuntimeLayer.hh>

namespace mikoto::runtime {

    /**
     * @brief Standalone player application without editor UI.
     * @details Borrows the window and project; both must outlive this application.
     * Owns the engine and game layer. Shutdown releases the game layer while
     * engine services are still alive and also handles a partially completed Init.
     */
    class RuntimeApp final : public core::Application, public platform::Subscriber {
    public:
        RuntimeApp( platform::IWindow* window, project::Project& project );

        auto Init() -> void override;
        auto Run() -> void override;
        auto Update() -> void override;
        auto Shutdown() -> void override;

    private:
        auto InitEventCallbacks() -> void;

        platform::IWindow* mWindow{};
        project::Project* mProject{};
        eastl::unique_ptr<core::Engine> mEngine{};
        RuntimeLayer* mGame{};
    };
}// namespace mikoto::runtime

#endif// MIKOTO_RUNTIME_APPLICATION_HH
