//
// Created by zanet on 10/1/2025.
//

#include <Core/Configuration.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Core/TimeService.hh>
#include <Audio/AudioService.hh>
#include <Filesystem/FileService.hh>
#include <Physics/PhysicService.hh>
#include <Threading/TaskService.hh>
#include <Core/Root.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

    auto Root::Init( const RootConfig &config ) -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing Root..." );

        // Time service
        TimeServiceCreateInfo timeServiceCreateInfo{
            .DefaultUnit{ TimeUnit::SECONDS }
        };
        TimeService *timeService{ s_Services.Register<TimeService>( timeServiceCreateInfo ) };
        timeService->Init();

        // Task service
        TaskServiceCreateInfo taskServiceCreateInfo{
            .WorkerThreadCount{ ThreadUtils::InferConcurrentThreads() }
        };
        TaskService *taskService{ s_Services.Register<TaskService>( taskServiceCreateInfo ) };
        taskService->Init();

        // Input service
        InputServiceCreateInfo inputServiceCreateInfo{
            .MainWindow{ config.TargetWindow }
        };
        InputService *inputService{ s_Services.Register<InputService>( inputServiceCreateInfo ) };
        inputService->Init();

        // Event service
        EventServiceCreateInfo eventServiceCreateInfo{};
        EventService *eventService{ s_Services.Register<EventService>( eventServiceCreateInfo ) };
        eventService->Init();

        // File service
        FileServiceCreateInfo fileServiceCreateInfo{};
        FileService *fileService{ s_Services.Register<FileService>( fileServiceCreateInfo ) };
        fileService->Init();

        // Physics service
        PhysicServiceCreateInfo physicsServiceCreateInfo{};
        PhysicService *physicService{ s_Services.Register<PhysicService>( physicsServiceCreateInfo ) };
        physicService->Init();

        // Audio service
        AudioServiceCreateInfo audioServiceCreateInfo{};
        AudioService* audioService{ s_Services.Register<AudioService>(audioServiceCreateInfo) };
        audioService->Init();

    }

    auto Root::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Shutting down Root..." );

        for (const auto &service: s_Services | std::views::values) {
            service->Shutdown();
        }
    }

    auto Root::StartFrame() -> void {}

    auto Root::EndFrame() -> void {}

    auto Root::UpdateState() -> void {
        // Update time step
        TimeService::Get().Update();
        const double timeStep{ TimeService::Get().GetTimeStep( TimeUnit::SECONDS ) };

        for (const auto &service: s_Services | std::views::values) {
            if (service->IsInitialized()) {
                service->Update( static_cast<float>( timeStep ) );
            }
        }
    }

}// namespace Mikoto