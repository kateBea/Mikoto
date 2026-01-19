//    Copyright 2025 ケイト
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

#ifndef MIKOTO_TASKGRAPH_HH
#define MIKOTO_TASKGRAPH_HH

#include <utility>

#include <taskflow/taskflow.hpp>

namespace Mikoto {
    class TaskGraph final {
    public:

        template <typename... Args>
        auto Emplace(Args&&... args) -> void {
            m_Taskflow.emplace( std::forward<Args>(args)... );
        }

        operator tf::Taskflow&() noexcept {
            return m_Taskflow;
        }

    private:
        tf::Taskflow m_Taskflow{};
    };
}


#endif //MIKOTO_TASKGRAPH_HH