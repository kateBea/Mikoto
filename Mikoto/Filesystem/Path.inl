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

#ifndef MIKOTO_PATH_INL
#define MIKOTO_PATH_INL

#include <string>
#include <string_view>
#include <filesystem>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Filesystem/Path.hh>

#include <Logging/Assert.hh>

namespace mikoto::filesystem {

    template<typename StringType>
    auto Path::GetPathTyped() const noexcept -> StringType {
        if constexpr ( std::is_same_v<StringType, eastl::string_view> ) {
            return mPathUtf8;
        }else if constexpr ( std::is_same_v<StringType, std::string_view> ) {
            return mPathUtf8.c_str();
        } else if constexpr ( std::is_same_v<StringType, eastl::string> ) {
            return mPathUtf8;
        } else if constexpr ( std::is_same_v<StringType, std::string> ) {
            return string::ToStd( mPathUtf8 );
        } else if constexpr ( std::is_same_v<StringType, const char*> ) {
            return mPathUtf8.c_str();
        } else if constexpr ( std::is_same_v<StringType, std::filesystem::path> ) {
            return std::filesystem::path{ mPathUtf8.c_str() };
        } else if constexpr ( std::is_same_v<StringType, std::wstring> ) {
            return std::filesystem::path{ mPathUtf8.c_str() }.wstring();
        } else if constexpr ( std::is_same_v<StringType, eastl::wstring> ) {
            return eastl::wstring{ string::ToWide( mPathUtf8 ) };
        } else {
            MKT_STATIC_ASSERT( false, "Not valid string type" );
        }

        // Make compiler happy
        return StringType{};
    }
}// namespace mikoto::filesystem

#endif
