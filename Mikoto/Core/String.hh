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

#ifndef MIKOTO_CORE_STRING_HH
#define MIKOTO_CORE_STRING_HH

#include <string>
#include <iterator>
#include <algorithm>
#include <string_view>

#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>

#include <fmt/format.h>

#include <Core/Profiler.hh>

namespace mikoto::string {

    enum class ComparePolicy {
        eIgnoreCase,
        eCaseSensitive
    };

    /**
     * @brief Copies an EASTL string into standard-library storage.
     * @param str Source character data.
     * @returns Copy of the same bytes, including embedded nulls.
     */
    [[nodiscard]] auto ConvertToStd( const eastl::string& str ) -> std::string;

    /**
     * @brief Copies a standard-library string into EASTL storage.
     * @param str Source character data.
     * @returns Copy of the same bytes, including embedded nulls.
     */
    [[nodiscard]] auto ConvertToEastl( const std::string& str ) -> eastl::string;

    /**
     * @brief Lowercases a character using the C locale.
     * @param c Character represented as unsigned char, or EOF, for the C-locale conversion.
     * @returns Lowercase character.
     */
    [[nodiscard]] auto ToLower( char c ) -> char;

    /**
     * @brief Converts UTF-8 text to a wide-character string.
     * @param str Source character data.
     * @returns UTF-16 wide-character representation.
     */
    [[nodiscard]] auto ToWide( eastl::string_view str ) -> eastl::wstring;

    /**
     * @brief Narrows wide characters by casting their code units to char.
     * @param str Null-terminated wide-character input.
     * @returns Narrowed code units; this does not perform Unicode transcoding.
     */
    [[nodiscard]] auto FromWChar( const wchar_t* str ) -> eastl::string;

    /**
     * @brief Narrows wide characters by casting their code units to char.
     * @param begin First wide character in the input range.
     * @param end One-past-last iterator of the borrowed range.
     * @returns Narrowed code units; this does not perform Unicode transcoding.
     */
    [[nodiscard]] auto FromWChar( const wchar_t* begin, const wchar_t* end ) -> eastl::string;

    /**
     * @brief Checks for the line-feed character.
     * @param value Value to inspect or edit.
     * @returns True for line feed.
     */
    [[nodiscard]] auto IsLineFeed( char value ) -> bool;

    /**
     * @brief Checks for ASCII decimal digits.
     * @param c Character to test for an ASCII decimal digit.
     * @returns True for a digit or a nonempty string containing only digits.
     */
    [[nodiscard]] auto IsNumber( char c ) -> bool;

    /**
     * @brief Checks for ASCII decimal digits.
     * @param value Value to inspect or edit.
     * @returns True for a digit or a nonempty string containing only digits.
     */
    [[nodiscard]] auto IsNumber( eastl::string_view value ) -> bool;

    /**
     * @brief Checks for an ASCII space character.
     * @param value Value to inspect or edit.
     * @returns True for space.
     */
    [[nodiscard]] auto IsSpace( char value ) -> bool;

    /**
     * @brief Copies the null-terminated buffer underlying a string view.
     * @param fmt Null-terminated input buffer; the view length is not used.
     * @returns Copied bytes up to the first null terminator.
     */
    [[nodiscard]] auto From( eastl::string_view fmt ) -> eastl::string;

    /**
     * @brief Tests whether a string contains a substring.
     * @param str Source character data.
     * @param lookUpString Substring to search for.
     * @returns True when the substring occurs.
     */
    [[nodiscard]] auto Contains( std::string_view str, std::string_view lookUpString ) -> bool;

    /**
     * @brief Formats and concatenates values without separators.
     * @tparam Args Types of the forwarded argument pack.
     * @param args Additional values forwarded to the operation.
     * @returns Concatenated standard-library string.
     */
    template<typename... Args>
    static auto Concat( Args&&... args ) -> std::string {
        MKT_BEGIN_PROFILER();
        fmt::memory_buffer buffer{};
        ( fmt::format_to( std::back_inserter( buffer ),
                          "{}", eastl::forward<Args>( args ) ),
          ... );

        return fmt::to_string( buffer );
    }

    /**
     * @brief Removes leading and trailing whitespace from a borrowed view.
     * @param sv Borrowed character range.
     * @returns Trimmed view sharing the input storage.
     */
    [[nodiscard]] auto Trim( eastl::string_view sv ) -> eastl::string_view;

    /**
     * @brief Returns true if two character sequences are equal.
     * @param str1 Null-terminated string to compare.
     * @param str2 Null-terminated string to compare.
     * @param policy Whether we want the comparison to be case-sensitive or not
     * @returns True if both strings are the same, false otherwise.
     */
    [[nodiscard]] auto Equal(
        const eastl::string_view str1,
        const eastl::string_view str2,
        ComparePolicy policy = ComparePolicy::eIgnoreCase ) -> bool;

    /**
     * @brief Checks whether a string begins with a prefix.
     * @param str Source character data.
     * @param other Prefix to test for.
     * @returns True when other is a prefix of str.
     */
    constexpr auto StartsWith( eastl::string_view str, eastl::string_view other ) -> bool {
        return str.starts_with( other );
    }

    /**
     * @brief Replaces every occurrence of one character in a string.
     * @tparam CharType Character type convertible to the string character type.
     * @param str Source character data.
     * @param oldVal Character to replace.
     * @param newVal Replacement character.
     */
    template<typename CharType>
    constexpr auto ReplaceWith( std::string& str, CharType oldVal, CharType newVal ) -> void {
        std::replace( str.begin(), str.end(), oldVal, newVal );
    }

    /**
     * @brief Formats arguments into an owned EASTL string.
     * @tparam Args Types of the forwarded argument pack.
     * @param fmt Format string, or source text for the From overload.
     * @param args Additional values forwarded to the operation.
     * @returns Formatted output.
     */
    template<typename... Args>
    [[nodiscard]] auto Format( fmt::format_string<Args...> fmt, Args&&... args ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        auto result{ fmt::format( fmt, eastl::forward<Args>( args )... ) };

        return eastl::string{ result.c_str() };
    }

}// namespace mikoto::string

#endif// MIKOTO_CORE_STRING_HH
