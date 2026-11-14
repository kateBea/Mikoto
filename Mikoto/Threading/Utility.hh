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

#ifndef MIKOTO_THREADING_UTILITY_HH
#define MIKOTO_THREADING_UTILITY_HH

#include <thread>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::threading {

    /**
     * @brief Get the number of concurrent hardware threads supported by the system
     * @return Number of concurrent threads supported by the system
     */
    MKT_NODISCARD auto GetThreadConcurrency() -> core::u32;

    /**
     * @brief Generates a hashed 32-bit unsigned integer ID from a thread ID.
     * @param id The standard thread identifier to be hashed.
     * @return A unique 32-bit unsigned integer hash representing the thread.
     */
    MKT_NODISCARD auto GetHashedID( std::thread::id id ) -> core::u32;
}// namespace mikoto::threading

#endif// MIKOTO_THREADING_UTILITY_HH
