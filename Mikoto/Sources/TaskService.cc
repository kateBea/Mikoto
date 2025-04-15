//
// Created by zanet on 3/26/2025.
//

#include <Threading/TaskService.hh>

namespace Mikoto {


    TaskService::TaskService( const TaskServiceCreateInfo &options )
        : m_ThreadCount{ options.WorkerThreadCount }
    {}

    auto TaskService::Init() -> void {
        m_TaskManager = CreateScope<TaskManager>( m_ThreadCount );
    }

    auto TaskService::Shutdown() -> void {
    }
}// namespace Mikoto