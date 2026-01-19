//
// Created by zanet on 3/27/2025.
//

#include <utility>
#include <ranges>

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
        std::future<int> future{ executor.async([](){
          std::cout << "async task returns 1\n";
          return 1;
        }) };
        executor.silent_async([](){ std::cout << "async task does not return\n"; });

        // create asynchronous tasks with dynamic dependencies
        tf::AsyncTask A{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK A" ); }) };
        tf::AsyncTask B{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK B" ); }, A) };
        tf::AsyncTask C{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK C" );  }, A) };
        tf::AsyncTask D{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK D" ); }, B, C ) };
    }

    TaskManager::TaskManager( const UInt32 threadCount )
        : m_ThreadCount( threadCount )
    {
    }

    auto TaskManager::Init() -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing TaskManager...");

#if !defined(NDEBUG)
        TestCode();
#endif

        m_IsInitialized = true;
    }

    auto TaskManager::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        for (auto& running : m_PeriodicTaskCondition | std::ranges::views::values) {
            running = false;
        }

        m_Executor.wait_for_all();

        MKT_CORE_LOGGER_DEBUG( "Shutting down TaskManager...");
    }

    auto TaskManager::SubmitTask( std::function<void()>&& task ) -> void {
        m_Executor.silent_async( std::move( task ) );
    }

    auto TaskManager::DisablePeriodicTask( const UInt32 index ) -> void {
        m_PeriodicTaskCondition.at( index ) = false;
    }

    auto TaskManager::ExecuteGraph( TaskGraph &graph ) -> void {
        m_Executor.run(graph).wait();
    }

    auto TaskManager::AddNewTaskRunner(std::function<void()>&& task, UInt32 index) -> void {
        m_Executor.silent_async( [func = std::move(task), this, index]() -> void {
            const Size frequency{ m_PeriodicTasksFrequency.at( index ) };

            while (m_PeriodicTaskCondition.at( index )) {
                if (func) {
                    func();
                }

                std::this_thread::sleep_for(std::chrono::seconds(frequency));
            }

            // FIXME:
            //m_PeriodicTasksFrequency.erase( index );
        });
    }
}// namespace Mikoto