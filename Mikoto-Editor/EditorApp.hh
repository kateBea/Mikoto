/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// Project Headers
#include <Common/Application.hh>
#include <Common/Configuration.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    class EditorApp final : public Application {
    public:

        auto Run(Int32 argc, char** argv) -> Int32 override;

    protected:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
