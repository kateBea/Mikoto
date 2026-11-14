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

#include <exception>

#include <imgui.h>

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Core/CVar.hh>
#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <Platform/CoreEvents.hh>

#include <Application/EditorApp.hh>

#if MIKOTO_MODULE_NETWORKING
#include <Networking/NetworkService.hh>
#endif

#include <Core/Types.hh>
#include <Core/TimeService.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Platform/Utility.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::renderer;

    EditorApp::EditorApp( platform::IWindow *window )
        : mWindow{ window } {
        MKT_ASSERT( mWindow, "Window must not be null" );
    }

    auto EditorApp::Init() -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "Initializing Editor application..." );

        const eastl::string_view isoLanguage{
            core::CVarRegistry::Get()->GetValue<eastl::string>( "language_iso", "en-us" )
        };
        const ISOLanguage iso{ core::InferIso( isoLanguage ) };

        const core::EngineDescription config{
            .mEnableAllServices = true,
            .mEnableAllSubsystems = true,
            .mWindow = mWindow,
            .mIsoLanguage = iso,
        };

        mEngine = eastl::make_unique<core::Engine>( config );
        mEngine->Initialize();

        mThemeManager = eastl::make_unique<ThemeManager>();
        mThemeManager->Initialize();

        // TODO: Find a cleaner way to do this
        // Used by the APP to display in the window header
        core::CVarRegistry::Get()->Register<eastl::string>( "project_name", "Project name.", "", core::CVarFlags::eArchive );

        InitEventCallbacks();
    }

    auto EditorApp::Run() -> void {
        while ( IsRunning() ) {
            Update();
            MKT_PROFILE_FRAME();
        }
    }

    auto EditorApp::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "Shutting down Editor App..." );

        mState = core::ApplicationStatus::eStopped;

        // Stop callbacks before releasing the editor objects they can access.
        if ( mEngine ) {
            mEngine->StopBackgroundWork();
        }

        std::exception_ptr failure{};

        try {
            ShutdownEditorResources();
        } catch ( ... ) {
            failure = std::current_exception();
        }

        // Complete engine teardown even if an editor resource failed to clean up.
        if ( mEngine ) {
            try {
                mEngine->Shutdown();
            } catch ( ... ) {
                if ( !failure ) {
                    failure = std::current_exception();
                }
            }

            mEngine.reset();
        }

        mWindow = nullptr;

        if ( failure ) {
            std::rethrow_exception( failure );
        }
    }

    auto EditorApp::ShutdownEditorResources() -> void {
        MKT_BEGIN_PROFILER();

        std::exception_ptr failure{};

        try {
            mLayers.OnDestroy();
        } catch ( ... ) {
            failure = std::current_exception();
        }

        // Themes must still be released if a layer's OnDestroy throws.
        if ( mThemeManager ) {
            try {
                mThemeManager->Shutdown();
            } catch ( ... ) {
                if ( !failure ) {
                    failure = std::current_exception();
                }
            }

            mThemeManager.reset();
        }

        if ( failure ) {
            std::rethrow_exception( failure );
        }
    }

    auto EditorApp::Update() -> void {
        MKT_BEGIN_PROFILER();
        core::TimeService::Get()->Tick();

#if MIKOTO_MODULE_NETWORKING
        if ( auto *network{ network::NetworkService::GetPtr() } ) {
            network->PollTasks();
        }
#endif

        if ( !mWindow->IsMinimized() ) {
            const double timeStep{ core::TimeService::Get()->GetTimeStep() };

            platform::EventSystem::Get()->PollEvents();
            renderer::RenderSystem::Get()->PrepareFrame();
            
            UpdateWindowTitleBar();

            mLayers.OnUpdate( core::as<core::f32>( timeStep ) );
            mEngine->Update( core::as<core::f32>( timeStep ) );

            // Submit frame to be processed and present
            renderer::RenderSystem::Get()->SubmitFrame();
            renderer::RenderSystem::Get()->PresentFrame();
        }
    }

    auto EditorApp::UpdateWindowTitleBar() -> void {
        // Custom chrome draws live statistics itself. Keep the OS caption stable
        // to avoid requesting native caption repaints while it is hidden.
        const auto hasCustomChrome{ mWindow->HasCustomTitleBar() };
        const auto isRefreshIntervalReached{ core::TimeService::Get()->GetTime( core::TimeUnit::eMilliseconds ) - mLastUpdateTime >= kUpdateInterval };

        if ( !hasCustomChrome && isRefreshIntervalReached ) {
            mLastUpdateTime = core::TimeService::Get()->GetTime( core::TimeUnit::eMilliseconds );
            mOriginalTitle = mWindow->GetTitle();

            // Used by the APP to display in the window header
            const auto &cVars{ core::CVarRegistry::Get() };
            const eastl::string projectName{ cVars.GetValue<eastl::string>( "project_name", "-" ) };
            const eastl::string projectNameFormatted{ projectName.empty() ? eastl::string{ "" }
                                                                          : string::Format( "| Project: {}", projectName ) };
            f32 frameRate{ ImGui::GetIO().Framerate };
#if !defined( NDEBUG )
            mWindow->SetTitle( string::Format( "{} | BuildType: DEBUG | FPS: {:.2f} {}", mOriginalTitle, frameRate, projectNameFormatted ) );
#else
            mWindow->SetTitle( string::Format( "{} | BuildType: RELEASE | FPS: {:.2f} {}", mOriginalTitle, frameRate, projectNameFormatted ) );
#endif
        }
    }

    auto EditorApp::InitEventCallbacks() -> void {
        MKT_BEGIN_PROFILER();

        AddHandler<platform::WindowCloseEvent>(
            [this]( platform::Event &event ) -> bool {
            // Layers can defer shutdown while the user resolves unsaved work.
            mLayers.OnEvent( event );

            if ( event.IsHandled() ) {
                return true;
            }

            mState = core::ApplicationStatus::eStopped;
            event.SetHandled( true );
            return true;
        } );

        AddHandler<platform::KeyPressedEvent>(
            [this]( platform::Event &event ) -> bool {
            mLayers.OnEvent( event );
            return event.IsHandled();
        } );

        AddHandler<platform::MouseButtonPressedEvent>(
            [this]( platform::Event &event ) -> bool {
            mLayers.OnEvent( event );
            return event.IsHandled();
        } );

        AddHandler<platform::MouseMovedEvent>(
            [this]( platform::Event &event ) -> bool {
            mLayers.OnEvent( event );
            return event.IsHandled();
        } );

        AddHandler<platform::MouseButtonReleasedEvent>(
            [this]( platform::Event &event ) -> bool {
            mLayers.OnEvent( event );
            return event.IsHandled();
        } );

        AddHandler<platform::ContentDroppedEvent>(
            [this]( platform::Event &event ) -> bool {
            mLayers.OnEvent( event );
            return event.IsHandled();
        } );

        // To debug gamepad input
        AddHandler<platform::GamepadButtonPressedEvent>(
            [this]( platform::Event &event ) -> bool {
                mLayers.OnEvent( event );
                return event.IsHandled();
            } );

        AddHandler<platform::GamepadAxisEvent>(
            [this]( platform::Event &event ) -> bool {
                mLayers.OnEvent( event );
                return event.IsHandled();
            } );
    }
}// namespace Mikoto