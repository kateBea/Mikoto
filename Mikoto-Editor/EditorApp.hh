/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// Project Headers
#include <Layer.hh>
#include <Common/Registry.hh>
#include <Common/Application.hh>
#include <Platform/Window/Window.hh>
#include <Core/CommandLineParser.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    class EditorApp final : public Application {
    public:
        /**
         * @brief Creates and initializes the editor app and runs the main loop.
         * @param argc Argument count.
         * @param argv List of null terminated c-strings command line arguments.
         * */
        auto Run(Int32_T argc, char** argv) -> Int32_T;

    protected:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;
        auto UpdateState() -> void override;

    private:
        auto InitLayers() -> void;
        auto DestroyLayers() -> void;
        auto UpdateLayers() const -> void;
        auto InstallEventCallbacks() -> void;
        auto ParseCommandLineArgs(Int32_T argc, char **argv) -> void;

    private:
        Registry<Layer> m_LayerRegistry{};

        Scope_T<Window> m_MainWindow{};
        Scope_T<CommandLineParser> m_CommandLineParser{};
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
