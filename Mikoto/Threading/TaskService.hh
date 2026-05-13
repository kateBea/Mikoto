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

#ifndef MIKOTO_TASK_SERVICE_HH
#define MIKOTO_TASK_SERVICE_HH

#include <EASTL/functional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>

#include <ankerl/unordered_dense.h>

#include <Core/Service.hh>
#include <Threading/TaskGraph.hh>
#include <Threading/TaskManager.hh>
#include <functional>
#include <utility>



namespace mikoto::threading {

    using namespace mikoto::core;

    struct TaskServiceCreateInfo {
        tf::Executor* mExecutor{};
    };

    class TaskService final : public IService, public Singleton<TaskService> {
    public:
        explicit TaskService( const TaskServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        template<typename KeyType, typename ValueType, typename Func>
        auto ParallelFor(const ankerl::unordered_dense::map<KeyType, ValueType>& map, Func&& func) -> void {
            tf::Taskflow flow{};
            for (const auto& [key, value]: map) {
                flow.emplace([key, value, f = std::forward<Func>(func)]() mutable {
                    f(key, value);
                });
            }
            mTaskManager->Execute( flow ).wait();
        }

        template<typename Func, typename... Args>
        auto Submit( Func&& func, Args&&... args ) -> void {
            auto newTask{
                std::function<void()>{
                    std::bind( std::forward<Func>( func ),
                    std::forward<Args>( args )... )
                }
            };

            mTaskManager->SubmitTask( std::move( newTask ) );
        }

        auto Submit( tf::Taskflow& flow, bool wait ) -> void {
            mTaskManager->Submit( flow, wait );
        }

        MKT_NODISCARD auto GetWorkersCount() const -> u32;

        ~TaskService() override = default;

    private:
        eastl::unique_ptr<TaskManager> mTaskManager{};
    };
}// namespace Mikoto

#endif // MIKOTO_TASK_SERVICE_HH
