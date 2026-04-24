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

#include <EASTL/unique_ptr.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

namespace mikoto::threading {

    TaskService::TaskService( const TaskServiceCreateInfo& info )
        : mTaskManager{ eastl::make_unique<TaskManager>( info.mExecutor )}
    {}

    auto TaskService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing TaskService...");

        if (mTaskManager) {
            mTaskManager->Initialize();
        }

        mIsInitialized = true;
    }

    auto TaskService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down TaskService..." );
    }

    auto TaskService::GetWorkersCount() const -> u32 {
        return mTaskManager->GetWorkersCount();
    }
}// namespace Mikoto