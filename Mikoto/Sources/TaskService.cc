//
// Created by zanet on 3/26/2025.
//

#include <Logging/Logger.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {


    TaskService::TaskService( const TaskServiceCreateInfo &options )
        : m_ThreadCount{ options.WorkerThreadCount }
    {}

    auto TaskService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing TaskService...");

        m_TaskManager = CreateScope<TaskManager>( m_ThreadCount );
        if (m_TaskManager) {
            m_TaskManager->Init();
        }

        SetupPeriodicTaskRunner();

        m_IsInitialized = true;
    }

    auto TaskService::SetupPeriodicTaskRunner() -> void {
        tf::Taskflow tf{};

        auto runner{ [this]() -> void{
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }};
    }

    auto TaskService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down TaskService..." );
    }

    auto TaskService::Update( float dt ) -> void {


    }
}// namespace Mikoto