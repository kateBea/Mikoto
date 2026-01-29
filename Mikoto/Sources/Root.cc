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

#include "Core/SystemStats.hh"
#include "Filesystem/FileWatcherService.hh"
#include "Scene/SceneManager.hh"

namespace Mikoto {

    auto Root::Init( const RootConfig &config ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing Root..." );

        if (config.EnableCoreServices) {
            RegisterDeferred<TimeService>( TimeServiceCreateInfo{} );
            RegisterDeferred<TaskService>( TaskServiceCreateInfo{ .WorkerThreadCount{ ThreadUtils::InferConcurrentThreads() } } );
        }

        for ( const auto &service: s_Services | std::views::values ) {
            service->Init();
        }

        //TaskManager::Get()->RunPeriodically( 3, []() -> void { SystemStats::Get()->Update(); } );
    }

    auto Root::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Shutting down Root..." );

        for (const auto& [id, service] : std::views::reverse(s_Services)) {
            service->Shutdown();
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