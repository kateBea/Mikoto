/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// C++ Standard Library
#include <vector>

// Project Headers
#include <STL/Utility/Types.hh>
#include <Common/Application.hh>
#include <Layers/EditorLayer.hh>

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
        auto Init(ApplicationData&& appSpec) -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;
        auto UpdateState() -> void override;
        auto Present() -> void;

    private:
        auto InitLayers() -> void;
        auto DestroyLayers() const -> void;
        auto UpdateLayers() const -> void;
        auto RenderImGuiFrame() const -> void;
        auto InstallEventCallbacks() -> void;

        auto ParseArguments(Int32_T argc, char **argv) -> void;

    private:
        Scope_T<EditorLayer> m_EditorLayer{};
        Ref_T<CommandLineParser> m_CommandLineParser{};
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
