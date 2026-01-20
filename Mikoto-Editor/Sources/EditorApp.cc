/**
 * EditorApp.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tracy/Tracy.hpp>
#include <utility>

// Project headers
#include <Application/EditorApp.hh>
#include <Assets/AssetsService.hh>
#include <Audio/AudioDevice.hh>
#include <Core/Configuration.hh>
#include <Core/CoreEvents.hh>
#include <Core/Profiler.hh>
#include <Core/Root.hh>
#include <Core/TimeService.hh>
#include <Layers/EditorLayer.hh>
#include <Logging/Logger.hh>

#include "Core/Timer.hh"
#include "Filesystem/FileWatch.hh"
#include "Renderer/Core/RenderService.hh"

namespace Mikoto {

    auto EditorApp::Run() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        while ( IsRunning() ) {
            Update();
        }
    }

    auto EditorApp::GetPrefabUri( const PrefabModels prefab ) -> const std::string & {
        return dynamic_cast<EditorApp *>( s_Instance )->m_PrefabModels[prefab];
    }

    auto EditorApp::InitPrefabs() -> void {
        m_PrefabModels = {
            { PrefabModels::CUBE, "Resources/Models/Prefabs/cube/gltf/scene.gltf" },
            { PrefabModels::CONE, "Resources/Models/Prefabs/cone/gltf/scene.gltf" },
            { PrefabModels::SPHERE, "Resources/Models/Prefabs/sphere/gltf/scene.gltf" },
            { PrefabModels::CYLINDER, "Resources/Models/Prefabs/cylinder/gltf/scene.gltf" },
            //{ PrefabModels::SPONZA, "Resources/Models/Prefabs/sponza/sponza.obj" }
        };

        TaskGraph loaders{};
        for ( const auto &val: m_PrefabModels | std::views::values ) {
            loaders.Emplace( [&]() -> void { AssetsService::Get()->LoadAsset<Model>( val ); } );
        }

        loaders.Dump(DumpDst::STANDARD_OUTPUT);

        TaskService::Get()->WaitForExecution( loaders );
    }

    auto EditorApp::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Mikoto Editor..." );

        const RootConfig config{
            .LockFrameRate{ false },
            .TargetWindow{ m_Window }
        };

        Root::Init( config );

        SetupEventCallbacks();

        InitPrefabs();
    }

    auto EditorApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Shutting down Mikoto Editor..." );

        m_LayerStack.Shutdown();

        Root::Shutdown();

        m_Window = nullptr;
    }

    auto EditorApp::Update() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        TimeService::Get()->UpdateTimeStep();

        if ( !m_Window->IsMinimized() ) {
            const double timeStep{ TimeService::Get()->GetTimeStep( TimeUnit::SECONDS ) };

            RenderService::Get()->PrepareFrame();

            m_LayerStack.OnUpdate( static_cast<float>( timeStep ) );

            Root::UpdateState( static_cast<float>( timeStep ) );

            RenderService::Get()->EndFrame();
            RenderService::Get()->PresentFrame();
        }
    }

    auto EditorApp::SetWindow( Window *window ) -> void {
        m_Window = window;
    }

    auto EditorApp::SetupEventCallbacks() -> void {

        AddHandler( EventType::WINDOW_CLOSE_EVENT,
                    [this]( Event &event ) -> bool {
                        m_State = ApplicationStatus::STOPPED;
                        event.SetHandled( true );
                        MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Handled Window Event close" );
                        return true;
                    } );

        AddHandler( EventType::KEY_PRESSED_EVENT,
                    [this]( Event &event ) -> bool {
                        m_LayerStack.OnEvent( event );
                        return event.IsHandled();
                    } );

        EventService::Get()->Subscribe( this );
    }
}// namespace Mikoto
