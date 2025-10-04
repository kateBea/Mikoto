/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// Project Headers
#include <Assets/Audio.hh>
#include <Common/Application.hh>
#include <Core/Configuration.hh>
#include <Core/EventService.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window.hh>

namespace Mikoto {
    class EditorApp final : public Application, public Subscriber {
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

#endif// MIKOTO_EDITOR_RUNNER_HH
