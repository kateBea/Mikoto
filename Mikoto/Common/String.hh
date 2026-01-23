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

#ifndef MIKOTO_STRING_HH
#define MIKOTO_STRING_HH

#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include <Common/Common.hh>

namespace Mikoto::StringUtil {

    template<typename... Args>
    MKT_NODISCARD std::string Format(fmt::format_string<Args...> fmt, Args&&... args) {
        return fmt::format(fmt, std::forward<Args>(args)...);
    }

    MKT_NODISCARD inline auto From( const std::string_view fmt) -> std::string {
        return std::string{ fmt.data() };
    }
}


#endif //MIKOTO_STRING_HH