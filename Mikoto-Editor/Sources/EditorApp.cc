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

#include <EASTL/string.h>
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
    using namespace mikoto::platform;

    EditorApp::EditorApp( Window *window )
        : mWindow{ window } {
        MKT_ASSERT( mWindow, "Window must not be null" );
    }

    auto EditorApp::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Mikoto Editor..." );

        const EngineDescription config{
            .mEnableAllServices = true,
            .mEnableAllSubsystems = true,
            .mWindow = mWindow };

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

            // Update window header with current FPS. Framerate is updated at intervals
            if (TimeService::Get()->GetTime( TimeUnit::eMilliseconds ) - mLastUpdateTime >= kUpdateInterval ) {
                mLastUpdateTime = TimeService::Get()->GetTime( TimeUnit::eMilliseconds );
                static eastl::string originalTitle{ mWindow->GetTitle() };

#if !defined(NDEBUG)
                mWindow->SetTitle( string::Format( "{} | BuildType: DEBUG | FPS: {:.2f}", originalTitle, 1.0f / timeStep ));
#else
                mWindow->SetTitle( string::Format( "{} | BuildType: RELEASE | FPS: {:.2f}", originalTitle, 1.0f / timeStep ));
#endif
            }

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
            [this]( IEvent &event ) -> bool {
                mState = ApplicationStatus::eStopped;
                event.SetHandled( true );
                return true;
            } );

        AddHandler<KeyPressedEvent>(
            [this]( IEvent &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );

        AddHandler<MouseButtonPressedEvent>(
            [this]( IEvent &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );

        AddHandler<MouseButtonReleasedEvent>(
            [this]( IEvent &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );

        AddHandler<ContentDroppedEvent>(
            [this]( IEvent &event ) -> bool {
                mLayerStack.OnEvent( event );
                return event.IsHandled();
            } );
    }
}// namespace Mikoto
