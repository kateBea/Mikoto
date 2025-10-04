//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_SANDBOX_APP_HH
#define MIKOTO_SANDBOX_APP_HH

#include <Assets/Audio.hh>
#include <Library/Utility/Types.hh>
#include <Common/Application.hh>
#include <Core/EventService.hh>
#include <Platform/Window.hh>

namespace Mikoto {

    class SandboxApp final : public Application, public Subscriber {
    public:

        auto Run(Int32 argc, char** argv) -> Int32 override;

    protected:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

    private:
        auto SetupEventCallbacks() -> void;

        Unique<Window> m_Window{};

        // For testing only
        AudioSourceHandle m_Target{};
        AudioSourceHandle m_SourceHandle{};
        AudioSourceHandle m_SourceHandle2{};
        auto TestCode() -> void;
    };
}


#endif//MIKOTO_SANDBOX_APP_HH
