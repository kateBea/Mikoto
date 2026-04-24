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

#include <ranges>

#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/CoreEvents.hh>
#include <Core/TimeService.hh>

#include <Logging/Logger.hh>

#include <Application/EditorApp.hh>

namespace mikoto::editor {

    using namespace mikoto::core;

    EditorApp::EditorApp( Window *window )
        : mWindow{ window }
    {}

    auto EditorApp::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Mikoto Editor..." );

        MKT_ASSERT( mWindow, "Window must not be null" );

        const EngineDescription config{
            .mEnableAllServices = true,
            .mEnableAllSubsystems = true,
            .mWindow = mWindow,
        };

        mEngine = eastl::make_unique<Engine>( config );
        mEngine->Initialize();

        mThemeManager = eastl::make_unique<ThemeManager>();
        mThemeManager->Initialize();

        InitEventCallbacks();
    }

    auto EditorApp::Run() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        while ( IsRunning() ) {
            Update();
        }
    }

    auto EditorApp::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Shutting down Editor App..." );

        mLayerStack.Shutdown();

        if (mThemeManager) {
            mThemeManager->Shutdown();
            mThemeManager.reset();
        }

        if (mEngine) {
            mEngine->Shutdown();
            mEngine.reset();
        }

        mWindow = nullptr;
    }

    auto EditorApp::Update() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Update the time step
        TimeService::Get()->Tick();

        if ( !mWindow->IsMinimized() ) {
            const double timeStep{ TimeService::Get()->GetTimeStep() };

            // Start a new frame
            RenderSystem::Get()->PrepareFrame();

            mLayerStack.OnUpdate( as<f32>( timeStep ) );
            mEngine->Update();

            // Submit frame to be processed and present
            RenderSystem::Get()->SubmitFrame();
            RenderSystem::Get()->PresentFrame();
        }
    }

    auto EditorApp::InitEventCallbacks() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        AddHandler<WindowCloseEvent>(
            [this]( Event &event ) -> bool {
                mState = ApplicationStatus::eStopped;
                event.SetHandled( true );
                return true;
            } );

        AddHandler<KeyPressedEvent>(
            [this]( Event &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );

        AddHandler<MouseButtonPressedEvent>(
            [this]( Event &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );

        AddHandler<MouseButtonReleasedEvent>(
            [this]( Event &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );
    }
}// namespace Mikoto
