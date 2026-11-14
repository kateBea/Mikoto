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

#ifndef MIKOTO_CORE_PROCESS_HH
#define MIKOTO_CORE_PROCESS_HH

#include <EASTL/string.h>
#include <EASTL/functional.h>

namespace mikoto::core::process {

    using AsyncCallback = eastl::function<void( const eastl::string& )>;

    /**
     * @brief Starts a command using the platform command processor.
     * @param command Command passed to the platform command processor.
     * @returns Status returned by the platform launch command.
     */
    auto RunDetached( const eastl::string& command ) -> int;

    /**
     * @brief Runs a command and collects standard output synchronously.
     * @param command Command passed to the platform command processor.
     * @returns Captured command output.
     */
    auto Run( const eastl::string& command ) -> eastl::string;

    /**
     * @brief Schedules command execution and forwards output to a callback.
     * @param command Command passed to the platform command processor.
     * @param onOutput Callback receiving captured command output.
     */
    auto RunAsync( const eastl::string& command, AsyncCallback&& onOutput ) -> void;

}// namespace mikoto::core::process

#endif// MIKOTO_CORE_PROCESS_HH
