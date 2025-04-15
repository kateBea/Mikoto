//
// Created by zanet on 1/26/2025.
//

#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>
#include <Audio/AudioService.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/ServiceManager.hh>
#include <FileSystem/FileService.hh>
#include <GUI/ImGuiService.hh>
#include <Assets/ShaderLibrary.hh>
#include <Memory/MemoryService.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/RenderService.hh>
#include <Threading/TaskService.hh>
#include <Timing/TimeService.hh>
#include <Core/LoggingService.hh>

#include "Renderer/FontService.hh"

namespace Mikoto {

    auto ServiceManager::Init(const EngineConfig& options) -> void {
        // Time service
        TimeServiceCreateInfo timeServiceCreateInfo{
            .DefaultUnit{ TimeUnit::SECONDS }
        };
        TimeService* timeService{ s_Registry.Register<TimeService>(timeServiceCreateInfo) };
        timeService->Init();

        // Logging service
        LoggingServiceDescription loggingServiceDescription{
            .LogFilePath{ options.Options.LogFilePath },
            .Severity{ LoggingSeverity::LOGGING_SEVERITY_DEBUG },
        };
        LoggingService* loggingService{ s_Registry.Register<LoggingService>(loggingServiceDescription) };
        loggingService->Init();

        // Runtime console service
        ConsoleManagerCreateInfo runtimeConsoleServiceCreateInfo{
            .Name{ "RuntimeConsole" },
        };
        RuntimeConsole* consoleManager{ s_Registry.Register<RuntimeConsole>(runtimeConsoleServiceCreateInfo) };
        consoleManager->Init();

        // Task system
        TaskServiceCreateInfo taskServiceCreateInfo{
            .WorkerThreadCount{ ThreadUtils::InferConcurrentThreads() }
        };
        TaskService* taskService{ s_Registry.Register<TaskService>(taskServiceCreateInfo) };
        taskService->Init();

        // Memory service
        MemoryServiceCreateInfo memoryServiceCreateInfo{
            .InitialMemoryPoolSize{ MKT_GIGABYTES( 1 ) }
        };
        MemoryService* memoryService{ s_Registry.Register<MemoryService>(memoryServiceCreateInfo) };
        memoryService->Init();

        // File service
        FileServiceCreateInfo fileServiceCreateInfo{};
        FileService* fileService{ s_Registry.Register<FileService>(fileServiceCreateInfo) };
        fileService->Init();

        // Input service
        InputServiceCreateInfo inputServiceCreateInfo{
            .MainWindow{ options.TargetWindow }
        };
        InputService* inputService{ s_Registry.Register<InputService>(inputServiceCreateInfo) };
        inputService->Init();

        // Render service
        // ImGui and the asset service must be initialized after the render system
        // because it requires a valid render context active
        RenderServiceCreateInfo renderServiceCreateInfo{
            .TargetWindow{ options.TargetWindow },
            .RendererAPI{ options.Options.RendererAPI }
        };
        RenderService* renderSystem{ s_Registry.Register<RenderService>(renderServiceCreateInfo) };
        renderSystem->Init();

        // Imgui service
        ImGuiServiceDescription imguiServiceCreateInfo{
            .ImGuiFiles{ options.Options.ImGuiConfigDir },
            .BackendApi{ options.Options.RendererAPI },
            .TargetWindow{ options.TargetWindow }
        };
        ImGuiService* imguiService{ s_Registry.Register<ImGuiService>(imguiServiceCreateInfo) };
        imguiService->Init();

        // Audio service
        AudioServiceCreateInfo audioServiceCreateInfo{};
        AudioService* audioService{ s_Registry.Register<AudioService>(audioServiceCreateInfo) };
        audioService->Init();

        // Assets service
        AssetsServiceDescription assetsServiceCreateInfo{
            .Device{ renderSystem->GetGpuDevice() },
            .AudDevice{ audioService->GetDevice() },
        };
        AssetsService* assetsService{ s_Registry.Register<AssetsService>(assetsServiceCreateInfo) };
        assetsService->Init();

        // Events service
        EventServiceCreateInfo eventServiceCreateInfo{};
        EventService* eventService{ s_Registry.Register<EventService>(eventServiceCreateInfo) };
        eventService->Init();

        // Physics service
        PhysicServiceCreateInfo physicsServiceCreateInfo{};
        PhysicService* physicService{ s_Registry.Register<PhysicService>(physicsServiceCreateInfo) };
        physicService->Init();

        // Font service
        FontServiceCreateInfo fontServiceCreateInfo{};
        FontService* fontService{ s_Registry.Register<FontService>(fontServiceCreateInfo) };
        fontService->Init();

        // Shader library
        ShaderLibraryDescription shaderLibraryDescription{
            .FilePath{ options.Options.ShadersPath },
            .Device{ renderSystem->GetGpuDevice() },
        };
        ShaderLibrary* shaderService{ s_Registry.Register<ShaderLibrary>(shaderLibraryDescription) };
        shaderService->Init();

        // Model importer library
        MeshFactoryCreateInfo meshFactoryCreateInfo{
            .ImportersCount{ taskService->GetInstance()->GetWorkersCount() },
            .Device{ renderSystem->GetGpuDevice() },
        };
        MeshFactory* meshFactory{ s_Registry.Register<MeshFactory>(meshFactoryCreateInfo) };
        meshFactory->Init();
    }

    auto ServiceManager::Update(double dt) -> void {

    }

    auto ServiceManager::StartFrame() -> void {
        const auto& renderSystem{ GetService<RenderService>()};
        const auto& guiSystem{ GetService<ImGuiService>() };

        guiSystem.PrepareFrame();
        renderSystem.PrepareFrame();
    }

    auto ServiceManager::EndFrame() -> void {
        const auto& renderSystem{ GetService<RenderService>() };
        const auto& guiSystem{ GetService<ImGuiService>() };

        guiSystem.EndFrame();
        renderSystem.EndFrame();
    }

    auto ServiceManager::Shutdown() -> void {

        AssetsService& assetsSystem{ GetService<AssetsService>() };
        RenderService& renderSystem{ GetService<RenderService>() };
        AudioService& audioSystem{ GetService<AudioService>() };
        PhysicService& physicsSystem{ GetService<PhysicService>() };
        ImGuiService& guiSystem{ GetService<ImGuiService>() };
        InputService& inputSystem{ GetService<InputService>() };
        FileService& fileSystem{ GetService<FileService>() };
        TimeService& timeSystem{ GetService<TimeService>() };
        TaskService& taskSystem{ GetService<TaskService>() };
        EventService& eventSystem{ GetService<EventService>() };

        // Shut down assets first to release resources
        assetsSystem.Shutdown();

        guiSystem.Shutdown();

        renderSystem.Shutdown();

        audioSystem.Shutdown();
        physicsSystem.Shutdown();
        inputSystem.Shutdown();
        fileSystem.Shutdown();
        timeSystem.Shutdown();
        taskSystem.Shutdown();
        eventSystem.Shutdown();

        s_Registry.Clear();
    }

}