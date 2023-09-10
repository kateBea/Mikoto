/**
 * Engine.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <stdexcept>

// Third-Party Libraries
#include <fmt/ranges.h>

// Project Headers
#include <Core/Engine.hh>
#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <Editor/EditorLayer.hh>


namespace Mikoto {
    auto Engine::Run(Int32_T argc, char** argv) -> Int32_T {
        ParseArguments(argc, argv);

        AppSpec appSpec{};
        appSpec.WindowWidth = 1920;
        appSpec.WindowHeight = 1080;
        appSpec.Name = "Mikoto Engine";
        appSpec.WorkingDirectory = std::filesystem::current_path();
        appSpec.Executable = m_CommandLineArgs[0];
        appSpec.RenderingBackend = GraphicsAPI::OPENGL_API;
        appSpec.CommandLineArguments =
                std::unordered_set<std::string>{ m_CommandLineArgs.begin(), m_CommandLineArgs.end() };
        appSpec.ShowGUI = true;

        auto& application{ Application::Get() };

        try {
            application.Init(std::move(appSpec));
            application.PushLayer(std::make_shared<EditorLayer>());

            while (application.IsRunning()) {
                application.UpdateState();
            }

            application.ShutDown();
        }
        catch(const std::exception& exception) {
            MKT_APP_LOGGER_CRITICAL("EXCEPT! Message: {}", exception.what());
            return 1;
        }
        catch(...) {
            MKT_APP_LOGGER_CRITICAL("EXCEPT! [UNKNOWN]");
            return 1;
        }

        return 0;
    }

    auto Engine::ParseArguments(Int32_T argc, char **argv) -> void {
        const auto LIMIT{ &argv[argc] };
        for ( ; argv < LIMIT; ++argv)
            m_CommandLineArgs.emplace_back(*argv);
    }
}