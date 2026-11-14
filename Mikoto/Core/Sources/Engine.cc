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


#include <Platform/Windows/Windows.hh>

#if MIKOTO_MODULE_ANIMATION
#include <Animation/AnimationSystem.hh>

#endif

#if MIKOTO_MODULE_ASSETS
#include <Assets/AssetRegistryService.hh>

#endif

#if MIKOTO_MODULE_AUDIO
#include <Audio/AudioService.hh>

#endif

#include <Core/Engine.hh>
#include <Core/Exception.hh>

#include <Platform/EventSystem.hh>

#if MIKOTO_MODULE_RENDERER
#include <Platform/InputSystem.hh>

#endif
#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

#if MIKOTO_MODULE_NETWORKING
#include <Networking/NetworkService.hh>

#endif

#if MIKOTO_MODULE_PHYSICS
#include <Physics/PhysicSystem.hh>

#endif

#if MIKOTO_MODULE_RENDERER
#include <Platform/Window.hh>

#endif

#if MIKOTO_MODULE_RENDERER
#include <Renderer/Core/RenderSystem.hh>

#endif

#if MIKOTO_MODULE_RENDERER
#include <Renderer/Particle/ParticleSystem.hh>

#endif

#if MIKOTO_MODULE_SCENE
#include <Scene/SceneManager.hh>

#endif

#if MIKOTO_MODULE_SCRIPTING
#include <Scripting/ScriptingService.hh>

#endif

#include <Threading/TaskService.hh>
#include <Threading/ProfilerObserver.hh>
#if MIKOTO_MODULE_VIDEO
#include <Video/VideoService.hh>
#endif

namespace mikoto::core {
    using namespace mikoto::filesystem;
    using namespace mikoto::threading;
#if MIKOTO_MODULE_AUDIO
    using namespace mikoto::audio;
#endif
#if MIKOTO_MODULE_NETWORKING
    using namespace mikoto::network;
#endif
#if MIKOTO_MODULE_RENDERER
    using namespace mikoto::renderer;
#endif
#if MIKOTO_MODULE_SCRIPTING
    using namespace mikoto::scripting;
#endif
#if MIKOTO_MODULE_ANIMATION
    using namespace mikoto::animation;
#endif
#if MIKOTO_MODULE_ASSETS
    using namespace mikoto::asset;
#endif
#if MIKOTO_MODULE_SCENE
    using namespace mikoto::scene;
#endif
#if MIKOTO_MODULE_PHYSICS
    using namespace mikoto::physics;
#endif

    Engine::Engine( const EngineDescription& config )
        : mOwnerThread{ std::this_thread::get_id() },
          mIsoLanguage{ config.mIsoLanguage },
          mEnableAllServices{ config.mEnableAllServices },
          mEnableAllSubsystems{ config.mEnableAllSubsystems },
          mEnableImGui{ config.mEnableImGui },
          mCompressTextures{ config.mCompressTextures },
          mWindow{ config.mWindow } {
        MKT_BEGIN_PROFILER();
#if defined( MIKOTO_ENABLE_TRACY )
        mExecutor.make_observer<threading::ProfilerObserver>();
#endif
    }

    auto Engine::CheckOwnerThread() const -> void {
        if ( std::this_thread::get_id() != mOwnerThread ) {
            MKT_THROW_LOGIC_ERROR( "Engine lifecycle and Update must run on the engine's owning thread" );
        }
    }

    auto Engine::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        CheckOwnerThread();

        if ( mInitialized ) {
            return;
        }

        if ( !mNodes.empty() ) {
            MKT_THROW_LOGIC_ERROR( "Engine initialization is already in progress" );
        }

#if MIKOTO_MODULE_RENDERER
        if ( mEnableAllSubsystems && !mWindow ) {
            MKT_THROW_INVALID_ARGUMENT( "Engine rendering subsystems require a window" );
        }
#endif

        MKT_CORE_LOGGER_DEBUG( "Initializing Engine..." );

        try {
            // "All" means all services available in this build's selected modules.
            if ( mEnableAllServices ) {
#if MIKOTO_MODULE_NETWORKING
                PushService<network::NetworkService>( false, network::NetworkServiceCreateInfo{} );
#endif

                PushService<TimeService>( true, TimeServiceCreateInfo{} );
                PushService<FileService>( false, filesystem::FileServiceCreateInfo{} );

#if MIKOTO_MODULE_AUDIO
                PushService<AudioService>( false, audio::AudioServiceCreateInfo{} );
#endif

                PushService<RuntimeConsole>( false, RuntimeConsoleCreateInfo{} );
                PushService<FileWatcherService>( false, filesystem::FileWatcherServiceCreateInfo{} );
                PushService<TaskService>( false, threading::TaskServiceCreateInfo{ .mExecutor = &mExecutor } );
                PushService<LocalizationService>( false,
                    LocalizationServiceCreateInfo{
                        .mLocalizationBasePath = "Resources/Localization",
                        .mDefaultLanguage = mIsoLanguage } );

#if MIKOTO_MODULE_SCENE
                PushService<scene::SceneManager>( false );
#endif

#if MIKOTO_MODULE_ASSETS
                // Asset initialization creates the dummy GPU texture. OpenGL
                // must do this on the same thread that initializes RenderSystem
                // and owns the window context, not in a dependency-ready worker.
                // The scheduler also preserves this affinity during shutdown.
                const bool assetsOnMainThread{
                    mWindow && mWindow->IsApi( renderer::rhi::GraphicsAPI::eOpenGL )
                };
                PushService<asset::AssetRegistryService>( assetsOnMainThread, 
                    asset::AssetRegistryServiceDescription{ 
                        .mCompressTextures = mCompressTextures 
                    } );
#endif
            }

            if ( mEnableAllSubsystems ) {
                PushService<platform::EventSystem>( true, platform::EventServiceCreateInfo{} );

#if MIKOTO_MODULE_RENDERER
                PushService<platform::InputSystem>( true, platform::InputServiceCreateInfo{ .mWindow = mWindow } );
#endif

                PushService<SystemStats>( false );

#if MIKOTO_MODULE_PHYSICS
                PushService<physics::PhysicSystem>( false, physics::PhysicServiceCreateInfo{} );
#endif

#if MIKOTO_MODULE_ANIMATION
                PushService<animation::AnimationSystem>( false, animation::AnimationSystemCreateInfo{} );
#endif

#if MIKOTO_MODULE_RENDERER
                PushService<renderer::ParticleSystem>( false, renderer::ParticleSystemCreateInfo{} );
#endif

#if MIKOTO_MODULE_RENDERER
                PushService<renderer::RenderSystem>( true, renderer::RenderSystemCreateInfo{
                    .mWindow = mWindow,
                    .mRefreshRate = renderer::rhi::RefreshRate::eSync,
                    .mApi = mWindow->GetApi(),
                    .mEnableImGui = mEnableImGui,
                                                           } );
#endif

#if MIKOTO_MODULE_SCRIPTING
                PushService<scripting::ScriptingService>( true,
                    scripting::ScriptingServiceDescription{ .mScriptBasePath = "Assets/Scripts" } );
#endif

#if MIKOTO_MODULE_VIDEO
                PushService<video::VideoService>( true );
#endif
            }

            // Validate BOTH graphs before starting anything. Initialization and
            // update are separate DAGs: for instance physics needs the renderer to create GPU
            // debug resources, but the renderer's tick waits for physics.
            BuildSchedules();

            mInitialization.Run( mExecutor );
            mInitialized = true;
        } catch ( ... ) {
            // Taskflow has joined the failing batch. Roll back every service
            // that became ready, preserving the original startup exception.
            try {
                Shutdown();
            } catch ( const std::exception& error ) {
                MKT_CORE_LOGGER_ERROR( "Engine startup rollback failed: {}", error.what() );
            } catch ( ... ) {
                MKT_CORE_LOGGER_ERROR( "Engine startup rollback failed with an unknown exception" );
            }

            throw;
        }
    }

    auto Engine::BuildSchedules() -> void {
        MKT_BEGIN_PROFILER();

        eastl::vector<ServiceScheduleNode> initialization( mNodes.size() );
        eastl::vector<ServiceScheduleNode> shutdown( mNodes.size() );
        eastl::vector<ServiceScheduleNode> execution{};
        eastl::vector<usize> updateIndices{};
        eastl::vector<usize> updateLookup( mNodes.size(), mNodes.size() );

        for ( usize index{}; index < mNodes.size(); ++index ) {
            const auto& node{ mNodes[index] };

            initialization[index].mName = node.mType.name();
            initialization[index].mMainThread = node.mRunsOnMainThread;

            shutdown[index].mName = node.mType.name();
            shutdown[index].mMainThread = node.mRunsOnMainThread;

            if ( node.mSubsystem ) {
                updateLookup[index] = execution.size();
                updateIndices.push_back( index );

                execution.push_back( ServiceScheduleNode{
                    .mName = node.mType.name(),
                    .mMainThread = node.mRunsOnMainThread } );
            }
            else if ( !node.mUpdateAfter.empty() ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "{} declares UpdateAfter but has no Update()", node.mType.name() ) );
            }
        }

        auto resolve{ [&]( std::type_index dependency, const ServiceNode& owner, cstr phase ) -> usize {
            const auto found{ mNodeIndices.find( dependency ) };

            if ( found == mNodeIndices.end() ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Missing {} dependency '{}' for '{}'", phase, dependency.name(), owner.mType.name() ) );
            }

            return found->second;
        } };

        for ( usize index{}; index < mNodes.size(); ++index ) {
            const auto& node{ mNodes[index] };

            for ( std::type_index type: node.mInitialization ) {
                const usize dependency{ resolve( type, node, "initialization" ) };
                initialization[index].mDependencies.push_back( dependency );

                // Lifetime reverses the edge: consumers release their resources
                // BEFORE the service which owns those resources shuts down.
                shutdown[dependency].mDependencies.push_back( index );
            }

            for ( std::type_index type: node.mUpdateAfter ) {
                const usize dependency{ resolve( type, node, "update" ) };

                if ( !mNodes[dependency].mSubsystem ) {
                    MKT_THROW_RUNTIME_ERROR( fmt::format( "Update dependency '{}' is not a subsystem", type.name() ) );
                }

                execution[updateLookup[index]].mDependencies.push_back( updateLookup[dependency] );
            }
        }

        if ( !execution.empty() && !mNodeIndices.contains( typeid( TimeService ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Subsystem updates require TimeService" );
        }

        mShutdownErrors.resize( mNodes.size() );

        // Build shutdown first so even a later graph validation failure can
        // discard constructed services in their dependency lifetime order.
        mShutdown.Build( { shutdown.data(), shutdown.size() },
            [this]( usize index ) {
            ShutdownService( index );
        }, "Service shutdown" );

        mInitialization.Build( { initialization.data(), initialization.size() },
            [this]( usize index ) {
            InitializeService( index );
        }, "Service initialization" );

        auto update{ [this, indices = eastl::move( updateIndices )]( usize index ) {
            auto* system{ mNodes[indices[index]].mSubsystem };

            if ( system->IsInitialized() && !system->Sleeping() ) {
                system->Update( mTimeStep );
            }
        } };

        mExecution.Build( { execution.data(), execution.size() },
            eastl::move( update ), "Service update" );

        MKT_CORE_LOGGER_DEBUG( "Service schedules ready; worker count: {}", mExecutor.num_workers() );
    }

    auto Engine::InitializeService( usize index ) -> void {
        auto& node{ mNodes[index] };

        try {
            if ( !node.mService->IsInitialized() ) {
                node.mService->Initialize();
            }

            if ( !node.mService->IsInitialized() ) {
                MKT_THROW_RUNTIME_ERROR( "Initialize returned without marking the service ready" );
            }

            node.mStarted = true;
        } catch ( ... ) {
            // A service may mark itself ready before a later operation throws.
            node.mStarted = node.mService->IsInitialized();
            MKT_CORE_LOGGER_ERROR( "Service initialization failed: {}", node.mType.name() );
            throw;
        }
    }

    auto Engine::ShutdownService( usize index ) -> void {
        auto& node{ mNodes[index] };

        try {
            if ( node.mStarted ) {
                node.mStarted = false;
                node.mService->Shutdown();
            }
        } catch ( ... ) {
            // Do not let Taskflow cancel other cleanup tasks. Each node owns a
            // separate error slot; the caller reports failure AFTER all joins.
            mShutdownErrors[index] = std::current_exception();
        }

        node.mSubsystem = nullptr;
        node.mService.reset();
    }

    auto Engine::StopBackgroundWork() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        CheckOwnerThread();
        mInitialized = false;

        // Stop watchers and worker services before draining jobs. Keep the
        // service objects alive until their consumers finish shutting down.
        for ( usize index{}; index < mNodes.size(); ++index ) {
            auto& node{ mNodes[index] };

            if ( node.mStarted &&
                 ( node.mType == typeid( filesystem::FileWatcherService ) ||
                     node.mType == typeid( threading::TaskService )
#if MIKOTO_MODULE_NETWORKING
                     || node.mType == typeid( network::NetworkService )
#endif
                         ) ) {
                // Retain the object for its consumers, but never call Shutdown
                // again in the destruction pass, even if this attempt fails.
                node.mStarted = false;

                try {
                    node.mService->Shutdown();
                } catch ( ... ) {
                    mShutdownErrors[index] = std::current_exception();
                }
            }
        }

        WaitForBackgroundTasks();
    }

    auto Engine::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Also joins final cleanup jobs submitted since the application stopped
        // background work and released its layers.
        StopBackgroundWork();
        mShutdown.Run( mExecutor );

        std::exception_ptr failure{};

        for ( usize index{}; index < mShutdownErrors.size(); ++index ) {
            if ( mShutdownErrors[index] ) {
                MKT_CORE_LOGGER_ERROR( "Service shutdown failed: {}", mNodes[index].mType.name() );

                if ( !failure ) {
                    failure = mShutdownErrors[index];
                }
            }
        }

        ClearServices();

        if ( failure ) {
            std::rethrow_exception( failure );
        }
    }

    auto Engine::ClearServices() -> void {
        // Callbacks keep service indices and pointers: discard them first.
        mInitialization.Clear();
        mExecution.Clear();
        mShutdown.Clear();
        mShutdownErrors.clear();
        mNodeIndices.clear();
        mNodes.clear();
    }

    auto Engine::WaitForBackgroundTasks() -> void {
        MKT_BEGIN_PROFILER();
        mExecutor.wait_for_all();
    }

    auto Engine::Update( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        CheckOwnerThread();

        if ( !mInitialized ) {
            return;
        }

        if ( auto* tasks{ threading::TaskService::GetPtr() }; tasks && tasks->IsInitialized() ) {
            tasks->ExecuteMainThreadTasks();
        }

#if MIKOTO_MODULE_ASSETS
        if ( auto* assets{ asset::AssetRegistryService::GetPtr() }; assets && assets->IsInitialized() ) {
            assets->UpdateStreaming();
        }
#endif

        // All tasks consume one frame timestep. The join prevents jobs from
        // racing the next frame's scene/UI edits or reusing this value early.
        mTimeStep = timeStep;
        mExecution.Run( mExecutor );
    }
}// namespace mikoto::core
