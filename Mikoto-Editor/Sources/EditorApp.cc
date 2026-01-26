//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include <Core/Root.hh>
#include <Core/Profiler.hh>
#include <Core/CoreEvents.hh>
#include <Core/TimeService.hh>
#include <Core/Configuration.hh>

#include <Logging/Logger.hh>

#include <Layers/EditorLayer.hh>
#include <Application/EditorApp.hh>

#include <Assets/AssetsService.hh>
#include <Renderer/Core/RenderService.hh>

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
        MKT_BEGIN_PROFILER_NAMED();

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

        TaskService::Get()->WaitForExecution( loaders );
    }

    auto EditorApp::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Mikoto Editor..." );

        const RootConfig config{
            .EnableImGui{ true },
            .LockFrameRate{ false },
            .EnableRenderService{ true },
            .TargetWindow{ m_Window },
            .TargetApi{ m_Window->GetApi() }
        };

        Root::Init( config );

        SetupEventCallbacks();

        InitPrefabs();
    }

    auto EditorApp::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        MKT_BEGIN_PROFILER_NAMED();

        m_Window = window;
    }

    auto EditorApp::SetupEventCallbacks() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
