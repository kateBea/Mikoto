//
// Created by zanet on 3/26/2025.
//

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {


    TaskService::TaskService( const TaskServiceCreateInfo &options )
        : m_ThreadCount{ options.WorkerThreadCount }
    {}

    auto TaskService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing TaskService...");

        m_TaskManager = CreateScope<TaskManager>( m_ThreadCount );
        if (m_TaskManager) {
            m_TaskManager->Init();
        }

        SetupPeriodicTaskRunner();

        m_IsInitialized = true;
    }

    auto TaskService::SetupPeriodicTaskRunner() -> void {

    }

    auto TaskService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down TaskService..." );
    }

    auto TaskService::Update( float ) -> void {


    }
}// namespace Mikoto