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

#ifndef MIKOTO_THREADING_TASK_SERVICE_HH
#define MIKOTO_THREADING_TASK_SERVICE_HH

#include <mutex>

#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/functional.h>

#include <ankerl/unordered_dense.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>

#include <Threading/TaskManager.hh>

namespace mikoto::threading {

    struct TaskServiceCreateInfo {
        tf::Executor* mExecutor{};
    };

    class TaskService final : public core::IService, public core::Singleton<TaskService> {
    public:
        explicit TaskService( const TaskServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto ExecuteTaskGraph( tf::Taskflow& flow, bool wait ) -> void;

        template<typename Func>
        auto PushMainThreadTask( Func&& func ) -> void {
            const std::lock_guard lock{ mMainThreadTasksMutex };

            if ( !mAcceptingMainThreadTasks ) {
                return;
            }

            mMainThreadTasks.emplace_back( eastl::forward<Func>( func ) );
        }

        auto ExecuteMainThreadTasks() -> void;

        template<typename Func, typename... Args>
        auto ExecuteWorkerTask( Func&& func, Args&&... args ) -> void {
            auto newTask{
                eastl::function<void()>{
                    std::bind( eastl::forward<Func>( func ),
                    eastl::forward<Args>( args )... )
                }
            };

            mTaskManager->SubmitTask( eastl::move( newTask ) );
        }

        template<typename KeyType, typename ValueType, typename Func>
        auto ExecuteParallelFor(const ankerl::unordered_dense::map<KeyType, ValueType>& map, Func&& func) -> void {
            tf::Taskflow flow{};
            for (const auto& [key, value]: map) {
                flow.emplace([key, value, f = eastl::forward<Func>(func)]() mutable {
                    f(key, value);
                });
            }

            mTaskManager->Execute( flow ).wait();
        }

        MKT_NODISCARD auto GetWorkersCount() const -> core::u32;

    private:
        // Publication and shutdown share one lock; callback execution never holds it.
        std::mutex mMainThreadTasksMutex{};

        eastl::unique_ptr<TaskManager> mTaskManager{};
        eastl::vector<eastl::function<void()>> mMainThreadTasks{};

        bool mAcceptingMainThreadTasks{};
    };
}// namespace mikoto::threading

#endif// MIKOTO_THREADING_TASK_SERVICE_HH
