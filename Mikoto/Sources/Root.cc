//
// Created by zanet on 10/1/2025.
//

#include <Assets/AssetsService.hh>
#include <Audio/AudioService.hh>
#include <Core/Configuration.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Core/Root.hh>
#include <Core/TimeService.hh>
#include <Filesystem/FileService.hh>
#include <ImGui/ImGuiService.hh>
#include <Logging/Logger.hh>
#include <Memory/MemoryService.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/RenderService.hh>
#include <Threading/TaskService.hh>
#include <Scripting/ScriptingService.hh>

#include <Threading/ThreadUtility.hh>

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

        // Memory service
        MemoryServiceCreateInfo memoryServiceCreateInfo{};
        MemoryService *memoryService{ s_Services.Register<MemoryService>( memoryServiceCreateInfo ) };
        memoryService->Init();

        //
        // Imgui service
        ImGuiServiceDescription imguiServiceCreateInfo{
            .Device{ renderSystem->GetGpuDevice() },
            .BackendApi{ GraphicsAPI::VULKAN_API },
            .TargetWindow{ config.TargetWindow }
        };
        ImGuiService *imguiService{ s_Services.Register<ImGuiService>( imguiServiceCreateInfo ) };
        imguiService->Init();

        // Assets service
        AssetsServiceDescription assetsServiceCreateInfo{
            .Device{ renderSystem->GetGpuDevice() },
            .AudDevice{ audioService->GetDevice() },
        };
        AssetsService *assetsService{ s_Services.Register<AssetsService>( assetsServiceCreateInfo ) };
        assetsService->Init();

        // Scripting service
        ScriptingServiceDescription luaServiceCreateInfo{
        };
        ScriptingService *scriptingService{ s_Services.Register<ScriptingService>( luaServiceCreateInfo ) };
        scriptingService->Init();

    }

    auto Root::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Shutting down Root..." );

        for (const auto& [id, system] : std::views::reverse(s_Services)) {
            // Services need to be shutdown in the order they were initialized
            // Registry does not guarantee any order for now
            system->Shutdown();
        }

        MKT_CORE_LOGGER_DEBUG( "Final shutdown at Root and resource count is {}", IResource::s_ResourceCount );
    }

    auto Root::StartFrame() -> void {
        RenderService::Get()->PrepareFrame();
        ImGuiService::Get()->PrepareFrame();
    }

    auto Root::EndFrame() -> void {
        ImGuiService::Get()->EndFrame();
        RenderService::Get()->EndFrame();
    }

    auto Root::UpdateState( const float timeStep ) -> void {

        for (const auto &service: s_Services | std::views::values ) {
            if (service->IsInitialized()) {
                service->Update( timeStep );
            }
        }
    }

}// namespace Mikoto