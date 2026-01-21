//    Copyright 2025 ケイト
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

#include "Core/SystemStats.hh"
#include "Filesystem/FileWatcherService.hh"
#include "Scene/SceneManager.hh"

namespace Mikoto {

    auto Root::Init( const RootConfig &config ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Root..." );

        TimeServiceCreateInfo timeServiceCreateInfo{
            .DefaultUnit{ TimeUnit::SECONDS }
        };
        TimeService *timeService{ s_Services.Register<TimeService>( timeServiceCreateInfo ) };
        timeService->Init();

        FileWatcherServiceCreateInfo fileWatcherServiceCreateInfo{};
        FileWatcherService *fileWatcherService{ s_Services.Register<FileWatcherService>( fileWatcherServiceCreateInfo ) };
        fileWatcherService->Init();

        TaskServiceCreateInfo taskServiceCreateInfo{
            .WorkerThreadCount{ ThreadUtils::InferConcurrentThreads() }
        };
        TaskService *taskService{ s_Services.Register<TaskService>( taskServiceCreateInfo ) };
        taskService->Init();

        InputServiceCreateInfo inputServiceCreateInfo{
            .MainWindow{ config.TargetWindow }
        };
        InputService *inputService{ s_Services.Register<InputService>( inputServiceCreateInfo ) };
        inputService->Init();

        EventServiceCreateInfo eventServiceCreateInfo{};
        EventService *eventService{ s_Services.Register<EventService>( eventServiceCreateInfo ) };
        eventService->Init();

        FileServiceCreateInfo fileServiceCreateInfo{};
        FileService *fileService{ s_Services.Register<FileService>( fileServiceCreateInfo ) };
        fileService->Init();

        PhysicServiceCreateInfo physicsServiceCreateInfo{};
        PhysicService *physicService{ s_Services.Register<PhysicService>( physicsServiceCreateInfo ) };
        physicService->Init();

        AudioServiceCreateInfo audioServiceCreateInfo{};
        AudioService *audioService{ s_Services.Register<AudioService>( audioServiceCreateInfo ) };
        audioService->Init();

        // Render service
        // ImGui and the asset service must be initialized after the render system
        // because it requires a valid render context active
        RenderServiceCreateInfo renderServiceCreateInfo{
            .TargetWindow{ config.TargetWindow },
            .RendererAPI{ config.TargetApi },
        };
        RenderService *renderSystem{ s_Services.Register<RenderService>( renderServiceCreateInfo ) };
        renderSystem->Init();

        MemoryServiceCreateInfo memoryServiceCreateInfo{};
        MemoryService *memoryService{ s_Services.Register<MemoryService>( memoryServiceCreateInfo ) };
        memoryService->Init();

        AssetsServiceDescription assetsServiceCreateInfo{
            .Device{ renderSystem->GetGpuDevice() },
            .AudDevice{ audioService->GetDevice() },
        };
        AssetsService *assetsService{ s_Services.Register<AssetsService>( assetsServiceCreateInfo ) };
        assetsService->Init();

        ScriptingServiceDescription luaServiceCreateInfo{
        };
        ScriptingService *scriptingService{ s_Services.Register<ScriptingService>( luaServiceCreateInfo ) };
        scriptingService->Init();

        ConsoleManagerCreateInfo consoleCreateInfo{
        };
        RuntimeConsole *runtimeConsole{ s_Services.Register<RuntimeConsole>( consoleCreateInfo ) };
        runtimeConsole->Init();

        NetworkServiceCreateInfo networkServiceCreate{
        };
        NetworkService *networkService{ s_Services.Register<NetworkService>( networkServiceCreate ) };
        networkService->Init();

        SceneManager *sceneManager{ s_Services.Register<SceneManager>() };
        sceneManager->Init();

        LocalizationServiceCreateInfo localizationServiceCreateInfo{
            .LocalizationRoot{ "Resources/Localization" },
            .DefaultLanguage{ ISOLanguage::ES_ES }
        };

        LocalizationService* localizationService{ s_Services.Register<LocalizationService>( localizationServiceCreateInfo ) };
        localizationService->Init();

        //TaskManager::Get()->RunPeriodically( 3, []() -> void { SystemStats::Get()->Update(); } );
    }

    auto Root::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Shutting down Root..." );

        for (const auto& [id, system] : std::views::reverse(s_Services)) {
            // Services need to be shutdown in the order they were initialized
            // Registry does not guarantee any order for now
            system->Shutdown();
        }

        MKT_CORE_LOGGER_DEBUG( "Final shutdown at Root and resource count is {}", IResource::s_ResourceCount );
    }

    auto Root::UpdateState( const float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Can be done on another thread
        SystemStats::Get()->Update();

        for (const auto &service: s_Services | std::views::values ) {
            if (service->IsInitialized() && !service->IsSleeping()) {
                service->Update( timeStep );
            }
        }
    }
}// namespace Mikoto