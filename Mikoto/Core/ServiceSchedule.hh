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

#ifndef MIKOTO_CORE_SERVICE_SCHEDULE_HH
#define MIKOTO_CORE_SERVICE_SCHEDULE_HH

#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>
#include <EASTL/unique_ptr.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::core {

    struct ServiceScheduleNode {
        cstr mName{ "Unnamed service" };
        bool mMainThread{ false };
        eastl::vector<usize> mDependencies{};
    };

    // Compiles a dependency DAG into reusable worker taskflows separated by
    // main-thread calls. Build/Clear/Run belong to the engine's owning thread;
    // Run joins every worker batch before returning, including on failure.
    class ServiceSchedule final {
    public:
        ServiceSchedule() = default;
        DISABLE_COPY_AND_MOVE_FOR( ServiceSchedule );

        auto Build( eastl::span<const ServiceScheduleNode> nodes, eastl::function<void( usize )> callback, cstr name ) -> void;
        auto Run( tf::Executor& executor ) -> void;
        auto Clear() -> void;

    private:
        struct Batch {
            usize mMainThreadNode{};
            eastl::unique_ptr<tf::Taskflow> mWorkers{};
        };

        eastl::function<void( usize )> mCallback{};
        eastl::vector<Batch> mBatches{};
    };
}// namespace mikoto::core

#endif// MIKOTO_CORE_SERVICE_SCHEDULE_HH
