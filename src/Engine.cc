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
        auto& application{ Application::Get() };

        try {
            application.Init();
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
}