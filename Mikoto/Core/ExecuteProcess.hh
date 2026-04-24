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

#ifndef MIKOTO_EXECUTE_PROCESS_HH
#define MIKOTO_EXECUTE_PROCESS_HH

#include <EASTL/functional.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::core::process {

    using AsyncCallback = eastl::function<void( const eastl::string& )>;

    auto RunDetached(const eastl::string& command) -> int;

    // Blocks the calling thread until it has finish reading from spawning process stdout
    auto Run(const eastl::string& command) -> eastl::string;

    // Runs asynchronously, for every line in the file from spawning process it runs the callback
    auto RunAsync(const eastl::string& command, AsyncCallback&& onOutput) -> void;

} // namespace Mikoto

#endif //MIKOTO_EXECUTE_PROCESS_HH
