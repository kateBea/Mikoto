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

#include <iostream>
#include <typeinfo>
#include <typeindex>

#include <Core/Core.hh>
#include <Core/Timer.hh>
#include <Core/Engine.hh>
#include <Core/Profiler.hh>
#include <Core/SystemStats.hh>
#include <Core/EventSystem.hh>
#include <Core/InputSystem.hh>
#include <Core/TimeService.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

#include <Threading/TaskService.hh>

#include <Audio/AudioService.hh>

#include <Animation/AnimationSystem.hh>

#include <Networking/NetworkService.hh>

#include <Scripting/ScriptingService.hh>

#include <Scene/SceneManager.hh>

#include <Assets/AssetsService.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Particle/ParticleSystem.hh>

#include <Physics/PhysicSystem.hh>

#include <Logging/Logger.hh>
#include <Memory/Allocator.hh>
#include <Threading/TaskGraph.hh>

namespace mikoto::core {

    using namespace mikoto::core;
    using namespace mikoto::imgui;
    using namespace mikoto::threading;
    using namespace mikoto::audio;
    using namespace mikoto::network;
    using namespace mikoto::renderer;
    using namespace mikoto::scripting;
    using namespace mikoto::animation;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::physics;

    Engine::Engine( const EngineDescription &config )
        : mEnableAllServices{ config.mEnableAllServices },
        mEnableAllSubsystems{ config.mEnableAllSubsystems },
        mWindow{ config.mWindow }
    {}

    auto Engine::Initialize() -> void {
        MKT_PROFILE_SCOPE();

        MKT_CORE_LOGGER_DEBUG( "Initializing Engine..." );

        if (mEnableAllServices) {
            PushMainThreadService<TimeService>( TimeServiceCreateInfo{} );

            PushService<FileService>( FileServiceCreateInfo{} );
            PushService<AudioService>( AudioServiceCreateInfo{} );
            PushService<RuntimeConsole>( RuntimeConsoleCreateInfo{} );
            PushService<FileWatcherService>( FileWatcherServiceCreateInfo{} );
            PushService<TaskService>( TaskServiceCreateInfo{ .mExecutor = MKT_ADDRESSOF( mExecutor ) } );

            PushService<LocalizationService>(LocalizationServiceCreateInfo{
                .mLocalizationBasePath{ "Resources/Localization" },
                .mDefaultLanguage = ISOLanguage::ES_ES,
            });
            PushService<SceneManager>();
            PushService<AssetsService>( AssetsServiceDescription{} );
        }

        if (mEnableAllSubsystems) {
            PushMainThreadService<EventSystem>( EventServiceCreateInfo{} );
            PushMainThreadService<InputSystem>( InputServiceCreateInfo{ .mWindow = mWindow } );

            PushService<SystemStats>();
            PushService<PhysicSystem>( PhysicServiceCreateInfo{} );
            PushService<NetworkSystem>( NetworkServiceCreateInfo{} );
            PushService<AnimationSystem>( AnimationSystemCreateInfo{} );
            PushService<ParticleSystem>( ParticleSystemCreateInfo{} );

            RenderSystemCreateInfo renderServiceCreateInfo{
                 .mWindow = mWindow,
                 .mRefreshRate = RefreshRate::eUnlimited,
                 .mApi = mWindow->GetApi(),
                 .mEnableImGui = true };
            PushMainThreadService<RenderSystem>( renderServiceCreateInfo );

            PushService<ScriptingService>( ScriptingServiceDescription{ .mScriptBasePath{ "Assets/Scripts" }} );
        }

        // Initialize main thread modules
        for (auto& [type, node] : mMainThreadNodes) {
            if (!node.mService->IsInitialized()) {
                node.mService->Initialize();
            }
        }

        BuildInitTasks();

        // Initialize rest of modules
        mExecutor.run(mInitTaskGraph).wait();

        // Prepare execution graph
        BuildExecutionTasks();
    }

    auto Engine::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Shutting down Engine..." );

        // Wait for all tasks to finish
        mExecutor.wait_for_all();

        // Clear rest of modules
        BuildShutdownTasks();
        mExecutor.run(mShutdownTaskGraph).wait();

        // Shutdown main thread modules afterward as they are initialized first
        for (auto& [type, node] : mMainThreadNodes) {
            if (node.mService->IsInitialized()) {
                node.mService->Shutdown();
            }
        }
    }

    auto Engine::Update() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Run main thread tasks
        const double timeStep{ TimeService::Get()->GetTimeStep( TimeUnit::eSeconds ) };
        for (auto& [type, node] : mMainThreadNodes) {
            if (!node.mIsSubsystem) {
                continue;
            }

            auto system{ checked_cast<ISubsystem*>( node.mService) };

            if (system->IsInitialized() && !system->Sleeping()) {
                system->Update( as<f32>( timeStep ) );
            }
        }

        // Dispatch rest of modules
        // Do I need to wait this?
        // For now, I am waiting because the system do not support multithreading yet
        // the idea would be submitting work to the systems so they are able to start working
        // when I call run()
        mExecutor.run(mExecTaskGraph).wait();
    }

    auto Engine::BuildInitTasks() -> void {
        // Construct tasks
        for (auto& [type, node] : mNodes) {
            mInitTasks[type] = mInitTaskGraph.emplace([service = node.mService]() {
                if (!service->IsInitialized()) {
                    service->Initialize();
                }
            });

            mInitTasks[type].name( type.name() );
        }

        // Add dependencies
        for (auto& [type, node] : mNodes) {
            for (const auto& dep : node.mConstructDeps) {
                // mInitTasks[dep] runs before mInitTasks[type]
                mInitTasks[dep].precede(mInitTasks[type]);
            }
        }

        // [DEBUG]
        mInitTaskGraph.name("InitTaskGraph" );
        mInitTaskGraph.dump(std::cout);
        std::cout.flush();
    }

    auto Engine::BuildShutdownTasks() -> void {
        // Construct tasks
        for (auto& [type, node] : mNodes) {
            mShutdownTasks[type] = mShutdownTaskGraph.emplace([service = node.mService]() {
                if (service->IsInitialized()) {
                    service->Shutdown();
                }
            });

            mShutdownTasks[type].name( type.name() );
        }

        // Add dependencies
        // They run in reverse order so dependent first, dependency second
        for (auto& [type, node] : mNodes) {
            for (const auto& dep : node.mConstructDeps) {
                // mInitTasks[dep] runs before mInitTasks[type]
                mShutdownTasks[type].precede(mShutdownTasks[dep]);
            }
        }

        // [DEBUG]
        mShutdownTaskGraph.name("ShutdownTaskGraph" );
        mShutdownTaskGraph.dump(std::cout);
        std::cout.flush();
    }

    auto Engine::BuildExecutionTasks() -> void {
        for (auto& [type, node] : mNodes) {
            if (!node.mIsSubsystem) {
                continue;
            }

            mExecTasks[type] = mExecTaskGraph.emplace([node]() -> void {
                const auto system{ as<ISubsystem*>( node.mService) };
                const auto deltaTime{ TimeService::Get()->GetTimeStep( TimeUnit::eSeconds ) };

                if (system->IsInitialized() && !system->Sleeping()) {
                    system->Update( as<f32>( deltaTime ) );
                }
            });

            mExecTasks[type].name( type.name() );
        }

        // Construct dependencies
        for (auto& [type, node] : mNodes) {
            for (const auto& dep : node.mExecutionDeps) {
                // mExecTasks[dep] runs before mExecTasks[type]
                mExecTasks[dep].precede(mExecTasks[type]);
            }
        }

        // [ DEBUG ]
        MKT_CORE_LOGGER_DEBUG( "Worker count: {}", mExecutor.num_workers());
        mExecTaskGraph.name( "ExecTaskGraph" );
        mExecTaskGraph.dump(std::cout);
        std::cout.flush();
    }
} // namespace mikoto