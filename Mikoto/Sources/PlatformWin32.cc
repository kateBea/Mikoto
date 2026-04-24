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

#include <Core/Platform.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <guiddef.h>
#include <combaseapi.h>

#include <EASTL/string.h>
#include <EASTL/array.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Platform/PlatformWin32.hh>

namespace mikoto::platform::windows {

    using namespace mikoto::core;

    auto GuidToString( const GUID &guid ) -> eastl::string {
        eastl::array<wchar_t, 256> buffer{};
        const int written{ StringFromGUID2(guid, buffer.data(), as<i32>(buffer.size())) };

        if (written == 0) {
            return "";
        }

        eastl::string result{};
        for (const auto &c : buffer) {
            result.push_back( as<char>( c ) );

            if (c == NULL) {
                break;
            }
        }

        return result;
    }
}

#endif
