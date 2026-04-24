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

#ifndef MIKOTO_THREAD_UTILITY_HH
#define MIKOTO_THREAD_UTILITY_HH

#include <thread>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::threading {

    using namespace core;

    /**
     * @brief Get the number of concurrent threads supported by the system
     * @return Number of concurrent threads supported by the system
     */
    MKT_NODISCARD auto inline GetThreadConcurrency() -> u32 {
        return std::thread::hardware_concurrency();
    }
}
#endif//MIKOTO_THREAD_UTILITY_HH
