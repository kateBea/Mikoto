/**
 * Engine.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// C++ Standard Library
#include <vector>

// Project Headers
#include <Common/Types.hh>
#include <Common/Application.hh>
#include <Layers/EditorLayer.hh>

namespace Mikoto {
    class EditorApp : public Application {
    public:
        /**
         * @brief Creates and initializes the editor app and runs the main loop.
         * @param argc Argument count.
         * @param argv List of null terminated c-strings command line arguments.
         * */
        auto Run(Int32_T argc, char** argv) -> Int32_T;

    protected:
        auto Init(AppSpec&& appSpec) -> bool override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;
        auto UpdateState() -> void override;

    private:
        auto CreateLayers() -> void;
        auto DestroyLayers() -> void;
        auto UpdateLayers() -> void;
        auto RenderImGuiFrame() -> void;
        auto InstallEventCallbacks() -> void;

        auto ParseArguments(Int32_T argc, char **argv) -> void;

    private:
        /** Stores the command line arguments. */
        std::vector<std::string> m_CommandLineArgs{};

        /** Holds the editor layer. */
        std::unique_ptr<EditorLayer> m_EditorLayer{};
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
