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

#ifndef MIKOTO_FILESYSTEM_PATH_INL
#define MIKOTO_FILESYSTEM_PATH_INL

#include <string>
#include <filesystem>
#include <string_view>
#include <type_traits>

#include <EASTL/string.h>
#include <EASTL/type_traits.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>

#include <Logging/Assert.hh>

#include <Filesystem/Path.hh>

namespace mikoto::filesystem {

    template<typename StringType>
    auto Path::GetPathTyped() const -> StringType {
        if constexpr ( eastl::is_same_v<StringType, eastl::string_view> ) {
            return mPathUtf8;
        } else if constexpr ( eastl::is_same_v<StringType, std::string_view> ) {
            return std::string_view{ mPathUtf8.data(), mPathUtf8.size() };
        } else if constexpr ( eastl::is_same_v<StringType, eastl::string> ) {
            return mPathUtf8;
        } else if constexpr ( eastl::is_same_v<StringType, std::string> ) {
            return std::string{ mPathUtf8.data(), mPathUtf8.size() };
        } else if constexpr ( eastl::is_same_v<StringType, core::cstr> ) {
            return mPathUtf8.c_str();
        } else if constexpr ( eastl::is_same_v<StringType, std::filesystem::path> ) {
            return mNativePath;
        } else if constexpr ( eastl::is_same_v<StringType, std::wstring> ) {
            return mNativePath.wstring();
        } else if constexpr ( std::is_same_v<StringType, eastl::wstring> ) {
            const auto wide{ mNativePath.wstring() };
            return eastl::wstring{ wide.data(), wide.size() };
        } else {
            MKT_STATIC_ASSERT( eastl::is_same_v<StringType, void>, "Unsupported Path string type" );
        }

        // Make compiler happy
        return StringType{};
    }
}// namespace mikoto::filesystem

#endif// MIKOTO_FILESYSTEM_PATH_INL
