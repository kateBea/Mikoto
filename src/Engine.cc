/**
 * Engine.cc
 * Created by kate on 6/7/23.
 * */

#include <fmt/ranges.h>

#include <Engine.hh>

#include <Core/Application.hh>
#include <Core/Logger.hh>

#include <Editor/EditorLayer.hh>


namespace kaTe {
    auto Engine::Run() -> kaTe::Int32_T {
        Application& application{ Application::Get() };

        try {
            application.Init();
            application.PushLayer(std::make_shared<EditorLayer>());

            while (application.IsRunning()) {
                application.UpdateState();
            }

            application.ShutDown();
        }
        catch(const std::exception& exception) {
            KATE_APP_LOGGER_CRITICAL("Engine exception thrown.\n Message: {}", exception.what());
            return 1;
        }
        catch(...) {
            KATE_APP_LOGGER_CRITICAL("Engine exception thrown.\n Unknown type of exception");
            return 1;
        }

        return 0;
    }
}