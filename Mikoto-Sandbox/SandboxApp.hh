//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_SANDBOX_APP_HH
#define MIKOTO_SANDBOX_APP_HH

#include <Assets/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Common/Application.hh>
#include <Core/EventService.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window.hh>
#include <Core/LayerStack.hh>

namespace Mikoto {

    class SandboxApp final : public Application, public Subscriber {
    public:

        auto Run() -> void override;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto SetWindow(Window* window) -> void;

    private:
        auto Update() -> void override;

    private:
        auto SetupEventCallbacks() -> void;

        Window* m_Window{};
    };
}


#endif//MIKOTO_SANDBOX_APP_HH
