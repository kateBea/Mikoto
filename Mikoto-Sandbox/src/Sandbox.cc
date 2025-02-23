//
// Created by kate on 11/11/23.
//

#include <Platform/Window/Window.hh>
#include <SandboxApp.hh>

namespace Mikoto {

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

        Init( );

        while (IsRunning()) {
            UpdateState();
        }

        return exitCode;
    }

    auto SandboxApp::Init( ) -> void {

    }

    auto SandboxApp::InstallEventCallbacks() -> void {

    }

    auto SandboxApp::Shutdown() -> void {
    }

    auto SandboxApp::UpdateState() -> void {
    }
}
