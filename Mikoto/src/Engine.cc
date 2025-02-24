//
// Created by zanet on 1/26/2025.
//

#include <ranges>

#include <Core/Engine.hh>
#include <Core/System/RenderSystem.hh>
#include <Core/System/EventSystem.hh>
#include <Core/System/TaskSystem.hh>
#include <Core/System/TimeSystem.hh>
#include <Core/System/InputSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/GUISystem.hh>
#include <Core/System/PhysicsSystem.hh>
#include <Core/System/AudioSystem.hh>
#include <Core/System/AssetsSystem.hh>

namespace Mikoto {

    auto Engine::Init(const EngineConfig& options) -> void {
        s_Options = options;

        EventSystem& eventSystem{ *s_Registry.Register<EventSystem>(options) };
        TaskSystem& taskSystem{ *s_Registry.Register<TaskSystem>(options) };
        TimeSystem& timeSystem{ *s_Registry.Register<TimeSystem>(options) };
        FileSystem& fileSystem{ *s_Registry.Register<FileSystem>(options) };
        InputSystem& inputSystem{ *s_Registry.Register<InputSystem>(options) };
        GUISystem& guiSystem{ *s_Registry.Register<GUISystem>(options) };
        PhysicsSystem& physicsSystem{ *s_Registry.Register<PhysicsSystem>(options) };
        AudioSystem& audioSystem{ *s_Registry.Register<AudioSystem>(options) };
        RenderSystem& renderSystem{ *s_Registry.Register<RenderSystem>(options) };
        AssetsSystem& assetsSystem{ *s_Registry.Register<AssetsSystem>(options) };

        eventSystem.Init();
        taskSystem.Init();
        timeSystem.Init();
        fileSystem.Init();
        inputSystem.Init();
        physicsSystem.Init();
        audioSystem.Init();

        renderSystem.Init();
        guiSystem.Init();
        assetsSystem.Init();
    }

    auto Engine::UpdateState() -> void {
        for ( const auto& system: s_Registry | std::views::values ) {
            system->Update();
        }
    }

    auto Engine::StartFrame() -> void {
        const auto& renderSystem{ *s_Registry.Get<RenderSystem>() };
        const auto& guiSystem{ *s_Registry.Get<GUISystem>() };

        guiSystem.PrepareFrame();
        renderSystem.PrepareFrame();
    }

    auto Engine::EndFrame() -> void {
        const auto& renderSystem{ *s_Registry.Get<RenderSystem>() };
        const auto& guiSystem{ *s_Registry.Get<GUISystem>() };

        guiSystem.EndFrame();
        renderSystem.EndFrame();
    }

    auto Engine::Shutdown() -> void {
        // It is appropriate to shut down the systems in reverse order

        AssetsSystem& assetsSystem{ *s_Registry.Get<AssetsSystem>() };
        RenderSystem& renderSystem{ *s_Registry.Get<RenderSystem>() };
        AudioSystem& audioSystem{ *s_Registry.Get<AudioSystem>() };
        PhysicsSystem& physicsSystem{ *s_Registry.Get<PhysicsSystem>() };
        GUISystem& guiSystem{ *s_Registry.Get<GUISystem>() };
        InputSystem& inputSystem{ *s_Registry.Get<InputSystem>() };
        FileSystem& fileSystem{ *s_Registry.Get<FileSystem>() };
        TimeSystem& timeSystem{ *s_Registry.Get<TimeSystem>() };
        TaskSystem& taskSystem{ *s_Registry.Get<TaskSystem>() };
        EventSystem& eventSystem{ *s_Registry.Get<EventSystem>() };

        // Shut down assets first to release resources
        assetsSystem.Shutdown();

        guiSystem.Shutdown();

        renderSystem.Shutdown();

        audioSystem.Shutdown();
        physicsSystem.Shutdown();
        inputSystem.Shutdown();
        fileSystem.Shutdown();
        timeSystem.Shutdown();
        taskSystem.Shutdown();
        eventSystem.Shutdown();

        s_Registry.Clear();
    }

}