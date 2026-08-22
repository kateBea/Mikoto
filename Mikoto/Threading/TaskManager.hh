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

#ifndef MIKOTO_TASK_MANAGER_HH
#define MIKOTO_TASK_MANAGER_HH

#include <taskflow/taskflow.hpp>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskGraph.hh>

namespace mikoto::threading {

    class TaskManager final : public core::IService, public core::Singleton<TaskManager> {
    public:
        explicit TaskManager( tf::Executor* executor );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Submit( tf::Taskflow& flow, bool wait ) -> void;
        auto SubmitTask( std::function<void()>&& task ) -> void;
        auto Execute( tf::Taskflow& flow ) -> tf::Future<void>;

        MKT_NODISCARD auto GetWorkersCount() const -> core::u32;

    private:
        tf::Executor* mExecutor{};
    };
}// namespace Mikoto


#endif//MIKOTO_TASK_MANAGER_HH
