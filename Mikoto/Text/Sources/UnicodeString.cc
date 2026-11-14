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

#include <Core/Profiler.hh>

#include <Text/Utility.hh>
#include <Text/UnicodeString.hh>

namespace mikoto::text {

    UnicodeString::UnicodeString( eastl::string_view utf8 ) {
        MKT_BEGIN_PROFILER();
        AppendUtf8( utf8 );
    }

    auto UnicodeString::Append( CodePoint codePoint ) -> void {
        MKT_BEGIN_PROFILER();
        mCodePoints.push_back( codePoint );
    }

    auto UnicodeString::AppendUtf8( eastl::string_view text ) -> void {
        MKT_BEGIN_PROFILER();
        // Every input byte produces at most one code point, including invalid
        // bytes. Reserve that upper bound once; repeated edits reuse capacity.
        const core::usize required{ mCodePoints.size() + text.size() };

        if ( required > mCodePoints.capacity() ) {
            mCodePoints.reserve( eastl::max( required, mCodePoints.capacity() * 2 ) );
        }

        ForEachUtf8CodePoint( text, [this]( core::u32 point ) {
            mCodePoints.emplace_back( point );
        } );
    }

    auto UnicodeString::AssignUtf8( eastl::string_view text ) -> void {
        MKT_BEGIN_PROFILER();
        Clear();
        AppendUtf8( text );
    }

    auto UnicodeString::Clear() -> void {
        MKT_BEGIN_PROFILER();
        mCodePoints.clear();
    }

    auto UnicodeString::Reserve( core::usize count ) -> void {
        MKT_BEGIN_PROFILER();
        mCodePoints.reserve( count );
    }

    auto UnicodeString::IsEmpty() const -> bool {
        MKT_BEGIN_PROFILER();
        return mCodePoints.empty();
    }

    auto UnicodeString::GetCodePointCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mCodePoints.size();
    }

    auto UnicodeString::GetCodePoint( core::usize index ) const -> CodePoint {
        MKT_BEGIN_PROFILER();
        return mCodePoints.at( index );
    }

    auto UnicodeString::GetCodePoints() const -> eastl::span<const CodePoint> {
        MKT_BEGIN_PROFILER();
        return { mCodePoints.data(), mCodePoints.size() };
    }

    auto UnicodeString::ToUtf8() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        eastl::string result;
        result.reserve( mCodePoints.size() );

        for ( const auto point: mCodePoints ) {
            const auto value{ point.GetValue() };

            if ( value < 0x80 ) {
                result.push_back( static_cast<char>( value ) );
            }
            else if ( value < 0x800 ) {
                result.push_back( static_cast<char>( 0xC0 | ( value >> 6 ) ) );
                result.push_back( static_cast<char>( 0x80 | ( value & 0x3F ) ) );
            }
            else if ( value < 0x10000 ) {
                result.push_back( static_cast<char>( 0xE0 | ( value >> 12 ) ) );
                result.push_back( static_cast<char>( 0x80 | ( ( value >> 6 ) & 0x3F ) ) );
                result.push_back( static_cast<char>( 0x80 | ( value & 0x3F ) ) );
            }
            else {
                result.push_back( static_cast<char>( 0xF0 | ( value >> 18 ) ) );
                result.push_back( static_cast<char>( 0x80 | ( ( value >> 12 ) & 0x3F ) ) );
                result.push_back( static_cast<char>( 0x80 | ( ( value >> 6 ) & 0x3F ) ) );
                result.push_back( static_cast<char>( 0x80 | ( value & 0x3F ) ) );
            }
        }

        return result;
    }

    auto UnicodeString::ToUtf16() const -> eastl::u16string {
        MKT_BEGIN_PROFILER();
        eastl::u16string result;
        result.reserve( mCodePoints.size() );

        for ( const auto point: mCodePoints ) {
            const auto value{ point.GetValue() };

            if ( value < 0x10000 ) {
                result.push_back( static_cast<char16_t>( value ) );
            }
            else {
                // UTF-16 stores supplementary scalars as two 10-bit payloads
                // in a high/low surrogate pair after removing the BMP offset.
                const auto supplementary{ value - 0x10000 };
                result.push_back( static_cast<char16_t>( 0xD800 | ( supplementary >> 10 ) ) );
                result.push_back( static_cast<char16_t>( 0xDC00 | ( supplementary & 0x3FF ) ) );
            }
        }

        return result;
    }


    auto UnicodeString::begin() const -> Container::const_iterator {
        MKT_BEGIN_PROFILER();
        return mCodePoints.begin();
    }

    auto UnicodeString::end() const -> Container::const_iterator {
        MKT_BEGIN_PROFILER();
        return mCodePoints.end();
    }
}
