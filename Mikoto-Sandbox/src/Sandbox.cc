//
// Created by kate on 11/11/23.
//

#include <SandboxApp.hh>

#include "Core/EventManager.hh"
#include "Core/TimeManager.hh"
#include "Threading/TaskManager.hh"

namespace Mikoto {

    static auto GetApplicationSpec(const std::vector<std::string>& args) -> AppSpec {
        return {
            .WindowWidth = 1920,
            .WindowHeight = 1080,
            .Name = "Mikoto Sandbox",
            .WorkingDirectory = std::filesystem::current_path(),
            .Executable = Path_T{ args[0] },
            .RenderingBackend = GraphicsAPI::VULKAN_API,
            .CommandLineArguments = { args.begin(), args.end() },
        };
    }

    auto SandboxApp::ParseArguments(Int32_T argc, char** argv) -> void {
        const auto limit{ std::addressof(argv[argc]) };
        for ( ; argv < limit; ++argv) {
            m_CommandLineArgs.emplace_back(*argv);
        }
    }

    auto SandboxApp::Run(Int32_T argc, char** argv) -> Int32_T {
        ParseArguments(argc, argv);

        ParseArguments(argc, argv);

        Int32_T exitCode{ EXIT_SUCCESS };
        auto appSpecs{ GetApplicationSpec(m_CommandLineArgs) };

        if (Init( std::move(appSpecs) )) {
            // TODO: run logic
            while (IsRunning()) {
                ProcessEvents();
                UpdateState();
            }
        }

        return exitCode;
    }

    auto SandboxApp::Init( AppSpec&& appSpec ) -> bool {
        m_Spec = std::move(appSpec);
        TimeManager::Init();

        // Set the assets root path (this path contains import files like shaders, prefabs, etc.)

        TaskManager::Init();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Executable                : {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Current working directory : {}", m_Spec.WorkingDirectory.string());
        MKT_APP_LOGGER_INFO("=================================================================");

        WindowProperties windowProperties{
            m_Spec.Name,
            m_Spec.RenderingBackend,
            m_Spec.WindowWidth,
            m_Spec.WindowHeight
        };

        windowProperties.AllowResizing(true);

        m_MainWindow = Window::Create(std::move(windowProperties));

        if (m_MainWindow) {
            m_MainWindow->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR("Could not create application main window!");
        }

        InstallEventCallbacks();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Init time {} seconds", TimeManager::GetTime());
        MKT_APP_LOGGER_INFO("=================================================================");

        return true;
    }

    auto SandboxApp::InstallEventCallbacks() -> void {
        EventManager::Subscribe(m_Guid.Get(),
                                 EventType::APP_CLOSE_EVENT,
                                 [this](Event& event) -> bool {
                                     m_State = Status::STOPPED;
                                     event.SetHandled(true);
                                     MKT_APP_LOGGER_WARN("Handled App Event close");
                                     return false;
                                 });

        EventManager::Subscribe(m_Guid.Get(),
                                 EventType::WINDOW_CLOSE_EVENT,
                                 [this](Event& event) -> bool {
                                     m_State = Status::STOPPED;
                                     event.SetHandled(true);
                                     MKT_APP_LOGGER_WARN("Handled Window Event close");
                                     return false;
                                 });

        EventManager::Subscribe(m_Guid.Get(),
                                 EventType::WINDOW_RESIZE_EVENT,
                                 [this](Event&) -> bool {
                                     m_State = m_MainWindow->IsMinimized() ? Status::IDLE : Status::RUNNING;
                                     MKT_APP_LOGGER_WARN("Handled Window Resize Event");
                                     return false;
                                 });
    }

    auto SandboxApp::Shutdown() -> void {
    }

    auto SandboxApp::ProcessEvents() -> void {
        EventManager::ProcessEvents();
        m_MainWindow->ProcessEvents();
    }

    auto SandboxApp::UpdateState() -> void {
    }
}
