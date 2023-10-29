/**
 * Engine.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <memory>
#include <stdexcept>

// Project Headers
#include <Core/Engine.hh>
#include <Core/Logger.hh>
#include <Core/Application.hh>

namespace Mikoto {
    auto Engine::Run(Int32_T argc, char** argv) -> Int32_T {
        ParseArguments(argc, argv);

        AppSpec appSpec{};
        appSpec.WindowWidth = 1920;
        appSpec.WindowHeight = 1080;
        appSpec.Name = "Mikoto Engine";
        appSpec.WorkingDirectory = std::filesystem::current_path();
        appSpec.Executable = Path_T{ m_CommandLineArgs[0] };
        appSpec.RenderingBackend = GraphicsAPI::VULKAN_API;
        appSpec.CommandLineArguments =
                std::unordered_set<std::string>{ m_CommandLineArgs.begin(), m_CommandLineArgs.end() };
        appSpec.WantGUI = true;
        appSpec.WantEditor = true;

        auto& application{ Application::Get() };

        try {
            application.Init(std::move(appSpec));

            while (application.IsRunning()) {
                // Update application layers
                application.UpdateState();

                // Swap buffers and present to screen
                application.Present();

                // Process events in queue pending
                application.ProcessEvents();
            }

            application.Shutdown();
        }
        catch(const std::exception& exception) {
            MKT_COLOR_STYLE_PRINT_FORMATTED(MKT_FMT_COLOR_RED, MKT_FMT_STYLE_BOLD, "EXCEPT! Message:\n{}", exception.what());
            return 1;
        }
        catch(...) {
            MKT_COLOR_STYLE_PRINT_FORMATTED(MKT_FMT_COLOR_RED, MKT_FMT_STYLE_BOLD, "EXCEPT! [UNKNOWN]");
            return 1;
        }

        return 0;
    }

    auto Engine::ParseArguments(Int32_T argc, char** argv) -> void {
        const auto limit{ std::addressof(argv[argc]) };
        for ( ; argv < limit; ++argv)
            m_CommandLineArgs.emplace_back(*argv);
    }
}