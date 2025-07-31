/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// Project Headers
#include <Common/Application.hh>
#include <Common/Configuration.hh>
#include <Core/ArgsParser.hh>
#include <Layers/Layer.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    class EditorApp final : public Application, public Subscriber {
    public:

        auto Run(Int32_T argc, char** argv) -> Int32_T override;

    protected:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

    private:
        auto InitLayers() -> void;
        auto DestroyLayers() -> void;
        auto UpdateLayers() const -> void;
        auto InstallEventCallbacks() -> void;
        auto SetupCmdArguments() -> void;
        auto CheckArguments(Int32_T argc, char **argv ) const -> void;

    private:

        Configuration m_Options{};

        Scope_T<Window> m_MainWindow{};
        Scope_T<ArgsParser> m_ArgsParser{};

        Registry<Layer> m_LayerRegistry{};
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
