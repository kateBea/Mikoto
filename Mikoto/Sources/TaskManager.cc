//
// Created by zanet on 3/27/2025.
//

#include <taskflow/taskflow.hpp>
#include "Threading/TaskManager.hh"

namespace Mikoto {

    static auto TestCode() -> void {
        tf::Executor executor;
        tf::Taskflow taskflow;
        // auto [A, B, C, D] = taskflow.emplace(  // create four tasks
        //   [] () { MKT_CORE_LOGGER_DEBUG( "Hello from TASK A" ); },
        //   [] () { MKT_CORE_LOGGER_DEBUG( "Hello from TASK B" ); },
        //   [] () { MKT_CORE_LOGGER_DEBUG( "Hello from TASK C" ); },
        //   [] () { MKT_CORE_LOGGER_DEBUG( "Hello from TASK D" ); }
        // );
        //
        // A.precede(B, C);  // A runs before B and C
        // D.succeed(B, C);  // D runs after  B and C

        //executor.async(taskflow);

        // create asynchronous tasks directly from an executor
        std::future<int> future = executor.async([](){
          std::cout << "async task returns 1\n";
          return 1;
        });
        executor.silent_async([](){ std::cout << "async task does not return\n"; });

        // create asynchronous tasks with dynamic dependencies
        tf::AsyncTask A = executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK A" ); });
        tf::AsyncTask B = executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK B" ); }, A);
        tf::AsyncTask C = executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK C" );  }, A);
        tf::AsyncTask D = executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK D" ); }, B, C);}

    TaskManager::TaskManager( UInt32 threadCount )
        : m_ThreadCount( threadCount )
    {
    }

    auto TaskManager::Init() -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing TaskManager...");
    }

    auto TaskManager::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing TaskManager...");
    }

    auto TaskManager::SubmitTask( std::function<void()> &&task ) -> void {
        tf::Taskflow taskflow{};
        m_Executor.silent_dependent_async([func = std::move(task)] { func(); });
    }
}// namespace Mikoto