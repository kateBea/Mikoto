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

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskManager.hh>

namespace mikoto::threading {

    static auto TestCode() -> void {
        tf::Executor executor{};
        tf::Taskflow taskflow{};

        {
            auto [A, B, C, D] = taskflow.emplace(// create four tasks
                    []() { MKT_CORE_LOGGER_DEBUG( "Hello from TASK A" ); },
                    []() { MKT_CORE_LOGGER_DEBUG( "Hello from TASK B" ); },
                    []() { MKT_CORE_LOGGER_DEBUG( "Hello from TASK C" ); },
                    []() { MKT_CORE_LOGGER_DEBUG( "Hello from TASK D" ); } );

            A.precede( B, C );// A runs before B and C
            D.succeed( B, C );// D runs after  B and C

            //executor.async(taskflow);

            // create asynchronous tasks directly from an executor
            std::future<int> future{ executor.async( []() {
                std::cout << "async task returns 1\n";
                return 1;
            } ) };
            executor.silent_async( []() { std::cout << "async task does not return\n"; } );
        }

        {
            // create asynchronous tasks with dynamic dependencies
            tf::AsyncTask A{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK A" ); }) };
            tf::AsyncTask B{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK B" ); }, A) };
            tf::AsyncTask C{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK C" );  }, A) };
            tf::AsyncTask D{ executor.silent_dependent_async([](){ MKT_CORE_LOGGER_DEBUG( "Hello from TASK D" ); }, B, C ) };
        }
    }

    TaskManager::TaskManager( tf::Executor* executor )
        : mExecutor( executor )
    {
    }

    auto TaskManager::Initialize() -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing TaskManager...");

#if !defined(NDEBUG)
        TestCode();
#endif

        mIsInitialized = true;
    }

    auto TaskManager::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down TaskManager...");

        mIsInitialized = false;
    }

    auto TaskManager::Submit( tf::Taskflow& flow, bool wait ) -> void {
        if (wait) {
            mExecutor->run( flow ).wait();
        } else {
            mExecutor->run( flow );
        }
    }

    auto TaskManager::SubmitTask( std::function<void()>&& task ) -> void {
        mExecutor->silent_async( std::move( task ) );
    }

    auto TaskManager::Execute( tf::Taskflow& flow ) -> tf::Future<void> {
        return mExecutor->run( flow );
    }

    auto TaskManager::GetWorkersCount() const -> u32 {
        return mExecutor->num_workers();
    }
}// namespace Mikoto