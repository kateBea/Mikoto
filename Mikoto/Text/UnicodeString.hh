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

#ifndef MIKOTO_TEXT_UNICODE_STRING_HH
#define MIKOTO_TEXT_UNICODE_STRING_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::text {

    class CodePoint {
    public:
        constexpr explicit CodePoint( core::u32 value )
            : mValue{ value <= 0x10FFFF &&
                !( value >= 0xD800 && value <= 0xDFFF ) ? value : 0xFFFD }
        {

        }

        MKT_NODISCARD constexpr auto GetValue() const -> core::u32 {
            return mValue;
        }

    private:
        core::u32 mValue{};
    };

    class UnicodeString {
    public:
        using Container = eastl::vector<CodePoint>;

        UnicodeString() = default;

        explicit UnicodeString( eastl::string_view utf8 );

        auto Append( CodePoint codePoint ) -> void;
        auto AppendUtf8( eastl::string_view text ) -> void;
        auto AssignUtf8( eastl::string_view text ) -> void;

        auto Clear() -> void;
        auto Reserve( core::usize count ) -> void;

        MKT_NODISCARD auto IsEmpty() const -> bool;
        MKT_NODISCARD auto GetCodePointCount() const -> core::usize;

        MKT_NODISCARD auto GetCodePoint( core::usize index ) const -> CodePoint;
        MKT_NODISCARD auto GetCodePoints() const -> eastl::span<const CodePoint>;

        MKT_NODISCARD auto ToUtf8() const -> eastl::string;
        MKT_NODISCARD auto ToUtf16() const -> eastl::u16string;

        auto begin() const -> Container::const_iterator;
        auto end() const -> Container::const_iterator;

    private:
        Container mCodePoints{};
    };
}// namespace mikoto::text

#endif// MIKOTO_TEXT_UNICODE_STRING_HH
