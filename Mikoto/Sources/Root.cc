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

#include <Assets/AssetsService.hh>
#include <Audio/AudioService.hh>
#include <Core/Configuration.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Core/LocalizationService.hh>
#include <Core/Profiler.hh>
#include <Core/Root.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/TimeService.hh>
#include <Filesystem/FileService.hh>
#include <ImGui/ImGuiService.hh>
#include <Logging/Logger.hh>
#include <Memory/MemoryService.hh>
#include <Networking/NetworkService.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/Core/RenderService.hh>
#include <Scripting/ScriptingService.hh>
#include <Threading/TaskService.hh>
#include <Threading/ThreadUtility.hh>

#include <Animation/AnimationSystem.hh>
#include "Core/SystemStats.hh"
#include "Filesystem/FileWatcherService.hh"
#include "Platform/WindowsService.hh"
#include "Scene/SceneManager.hh"

namespace Mikoto {

#define SHUTDOWN_SERVICE( ServiceType )                                    \
    do {                                                                   \
        if ( auto ptr{ s_Services.Get<ServiceType>() }; ptr != nullptr ) { \
            ptr->Shutdown();                                               \
        }                                                                  \
    } while ( 0 )

#define SHUTDOWN_SUBSYSTEM( SubsystemType )                    \
    do {                                                       \
        if ( auto ptr{ s_Subsystems.Get<SubsystemType>() } ) { \
            ptr->Shutdown();                                   \
        }                                                      \
    } while ( 0 )


    auto Root::Init( const RootConfig &config ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Root..." );

        if (config.EnableAllServices) {
            PushService<TimeService>( TimeServiceCreateInfo{} );
            PushService<WindowsService>( WindowsServiceCreateInfo{} );
            PushService<FileService>( FileServiceCreateInfo{} );
            PushService<AudioService>( AudioServiceCreateInfo{} );
            PushService<RuntimeConsole>( ConsoleManagerCreateInfo{} );
            PushService<FileWatcherService>( FileWatcherServiceCreateInfo{} );

            PushService<LocalizationService>(LocalizationServiceCreateInfo{
                .LocalizationRoot{ "Resources/Localization" },
                    .DefaultLanguage{ ISOLanguage::ES_ES }
            });
        }

        if (config.EnableAllSubsystems) {
            PushSubsystem<SystemStats>();
            PushSubsystem<EventService>( EventServiceCreateInfo{} );
            PushSubsystem<PhysicService>( PhysicServiceCreateInfo{} );
            PushSubsystem<NetworkService>( NetworkServiceCreateInfo{} );
            PushSubsystem<ScriptingService>( ScriptingServiceDescription{} );
            PushSubsystem<AnimationSystem>( AnimationSystemCreateInfo{} );

            PushSubsystem<TaskService>( TaskServiceCreateInfo{ .WorkerThreadCount{ ThreadUtils::InferConcurrentThreads() } } );
        }

        for ( const auto &service: s_Services | std::views::values ) {
            service->Init();
        }

        for ( const auto &service: s_Subsystems | std::views::values ) {
            service->Init();
        }
    }

    auto Root::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Shutting down Root..." );

        SHUTDOWN_SERVICE(TimeService);
        SHUTDOWN_SERVICE(FileService);
        SHUTDOWN_SERVICE(AudioService);
        SHUTDOWN_SERVICE(RuntimeConsole);
        SHUTDOWN_SERVICE(FileWatcherService);
        SHUTDOWN_SERVICE(LocalizationService);

        SHUTDOWN_SUBSYSTEM(SystemStats);
        SHUTDOWN_SUBSYSTEM(EventService);
        SHUTDOWN_SUBSYSTEM(PhysicService);
        SHUTDOWN_SUBSYSTEM(NetworkService);
        SHUTDOWN_SUBSYSTEM(ScriptingService);
        SHUTDOWN_SUBSYSTEM(TaskService);

        SHUTDOWN_SUBSYSTEM(AnimationSystem);
        SHUTDOWN_SERVICE(SceneManager);
        SHUTDOWN_SERVICE(AssetsService);

        SHUTDOWN_SUBSYSTEM(RenderService);
        SHUTDOWN_SUBSYSTEM(InputService);

        SHUTDOWN_SERVICE(WindowsService);

        MKT_CORE_LOGGER_DEBUG( "Final shutdown at Root and resource count is {}", IResource::s_ResourceCount );
    }

    auto Root::UpdateSubsystems(double timeStep) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for (const auto &service: s_Subsystems | std::views::values ) {
            if (service->IsInitialized() && !service->IsSleeping()) {
                service->Update( timeStep );
            }
        }
    }

    auto Root::EnableRenderSubsystems( Window *window ) -> void {
        RegisterSubsystem<InputService>( InputServiceCreateInfo{ .MainWindow{ window } } );

        RenderServiceCreateInfo renderServiceCreateInfo{
            .TargetWindow{ window },
            .RendererAPI{ window->GetApi() },
            .EnableImGui{ true },
        };

        RegisterSubsystem<RenderService>( renderServiceCreateInfo );
        RegisterService<AssetsService>( AssetsServiceDescription{} );

        // FIXME: because service are shutdown in reverse order i must free the scenes first
        RegisterService<SceneManager>();
    }
}