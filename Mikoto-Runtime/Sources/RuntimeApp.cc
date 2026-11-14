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

#include <chrono>
#include <thread>
#include <exception>

#include <EASTL/algorithm.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Platform/CoreEvents.hh>

#include <Application/RuntimeApp.hh>

#if MIKOTO_MODULE_NETWORKING
#include <Networking/NetworkService.hh>

#endif
#include <Core/TimeService.hh>

#include <Platform/Event.hh>
#include <Platform/Utility.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::runtime {
    using namespace core;
    using namespace renderer;

    RuntimeApp::RuntimeApp( platform::IWindow* window, project::Project& project )
        : mWindow{ window }, mProject{ &project } {
        if ( !mWindow ) {
            MKT_THROW_RUNTIME_ERROR( "The runtime requires a window." );
        }
    }

    auto RuntimeApp::Init() -> void {
        MKT_BEGIN_PROFILER();

        const core::EngineDescription description{
            .mEnableAllServices = true,
                .mEnableAllSubsystems = true,
                    .mWindow = mWindow,
                        .mEnableImGui = false,
                            .mCompressTextures = mProject->GetCompressTextures()
        };
        mEngine = eastl::make_unique<core::Engine>( description );
        mEngine->Initialize();

        InitEventCallbacks();
        mGame = mLayers.PushLayer<RuntimeLayer>( mWindow, *mProject );
    }

    auto RuntimeApp::InitEventCallbacks() -> void {
        AddHandler<platform::WindowCloseEvent>( [this]( platform::Event& event ) {
            mState = core::ApplicationStatus::eStopped;
            event.SetHandled( true );
            return true;
        } );
    }

    auto RuntimeApp::Run() -> void {
        while ( IsRunning() ) {
            Update();
            MKT_PROFILE_FRAME();
        }
    }

    auto RuntimeApp::Update() -> void {
        MKT_BEGIN_PROFILER();
#if MIKOTO_MODULE_NETWORKING
        // Poll before scene/UI work, including frames when the window is minimized.
        if ( auto* network{ network::NetworkService::GetPtr() } ) {
            network->PollTasks();
        }
#endif

        core::TimeService::Get()->Tick();
        platform::PollWindowEvents();
        platform::EventSystem::Get()->PollEvents();

        if ( !IsRunning() ) {
            return;
        }

        if ( mWindow->IsMinimized() || mWindow->GetWidth() <= 0 || mWindow->GetHeight() <= 0 ) {
            mGame->CancelInput();
            return;
        }

        const core::f32 timeStep{ eastl::min( core::as<core::f32>( core::TimeService::Get()->GetTimeStep() ), 0.1f ) };
        auto* render{ renderer::RenderSystem::GetPtr() };

        render->PrepareFrame();
        mLayers.OnUpdate( timeStep );
        mEngine->Update( timeStep );

        render->SubmitFrame();
        render->PresentFrame();
    }

    auto RuntimeApp::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mEngine ) {
            return;
        }

        mState = core::ApplicationStatus::eStopped;

        mEngine->StopBackgroundWork();

        std::exception_ptr failure{};

        try {
            // Includes layers whose OnCreate failed partway through.
            mLayers.OnDestroy();
        } catch ( ... ) {
            failure = std::current_exception();
        }

        mGame = nullptr;

        if ( auto* events{ platform::EventSystem::GetPtr() } ) {
            events->UnSubscribe( this );
        }

        // Keep dependency teardown unconditional when layer cleanup fails.
        try {
            mEngine->Shutdown();
        } catch ( ... ) {
            if ( !failure ) {
                failure = std::current_exception();
            }
        }

        mEngine.reset();

        if ( failure ) {
            std::rethrow_exception( failure );
        }
    }
}// namespace mikoto::runtime
