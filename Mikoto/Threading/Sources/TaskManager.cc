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

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskManager.hh>

namespace mikoto::threading {

    TaskManager::TaskManager( tf::Executor* executor )
        : mExecutor( executor ) {
    }

    auto TaskManager::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "Initializing TaskManager..." );

        mIsInitialized = true;
    }

    auto TaskManager::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down TaskManager..." );

        mIsInitialized = false;
    }

    auto TaskManager::Submit( tf::Taskflow& flow, bool wait ) -> void {
        MKT_BEGIN_PROFILER();

        if ( wait ) {
            // Returns -1 when the caller is not
            // one of this executor's workers.
            if ( mExecutor->this_worker_id() >= 0 ) {

                // Keep this worker available to execute child
                // tasks while waiting for their graph.
                mExecutor->corun( flow );
            }
            else {
                mExecutor->run( flow ).get();
            }
        }
        else {
            mExecutor->run( flow );
        }
    }

    auto TaskManager::SubmitTask( std::function<void()>&& task ) -> void {
        MKT_BEGIN_PROFILER();
        mExecutor->silent_async( eastl::move( task ) );
    }

    auto TaskManager::Execute( tf::Taskflow& flow ) -> tf::Future<void> {
        MKT_BEGIN_PROFILER();
        return mExecutor->run( flow );
    }

    auto TaskManager::GetWorkersCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mExecutor->num_workers();
    }
}// namespace mikoto::threading
