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

#include <algorithm>
#include <cctype>
#include <format>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <array>

#include <Common/Common.hh>
#include <Math/Math.hh>
#include <Library/Utility/Types.hh>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace Mikoto::StringUtil {

    template<int Count>
    class StaticString {
    public:
        explicit constexpr StaticString() = default;

        explicit constexpr StaticString(const char* str) {
            Size i{};
            for (; i < Count - 1 && str[i] != '\0'; ++i) {
                m_Buffer[i] = str[i];
            }
            m_Buffer[i] = '\0';
        }

        explicit constexpr StaticString(std::string_view str) {
            const Size len{ Math::Min<Size>(str.size(), Count - 1) };
            std::copy_n(str.data(), len, m_Buffer.data());
            m_Buffer[len] = '\0';
        }

        MKT_NODISCARD constexpr auto GetData() -> char* {
            return m_Buffer.data();
        }

        MKT_NODISCARD constexpr auto GetData() const -> const char* {
            return m_Buffer.data();
        }

        MKT_NODISCARD constexpr auto C_Str() const -> const char* {
            return m_Buffer.data();
        }

        MKT_NODISCARD constexpr auto GetSize() const -> Size {
            return std::char_traits<char>::length(m_Buffer.data());
        }

        MKT_NODISCARD constexpr auto GetCapacity() const -> Size {
            return Count;
        }

        MKT_NODISCARD constexpr auto IsEmpty() const -> bool {
            return m_Buffer[0] == '\0';
        }

        MKT_NODISCARD constexpr auto operator[](Size index) -> char& {
            return m_Buffer[index];
        }

        MKT_NODISCARD constexpr auto operator[](Size index) const -> const char& {
            return m_Buffer[index];
        }

        MKT_NODISCARD constexpr auto GetView() const -> std::string_view {
            return std::string_view{ m_Buffer.data(), GetSize() };
        }

        MKT_NODISCARD constexpr auto operator==(const StaticString& other) const -> bool {
            return GetView() == other.GetView();
        }

        MKT_NODISCARD constexpr auto operator!=(const StaticString& other) const -> bool {
            return !(*this == other);
        }

    private:
        std::array<char, Count> m_Buffer{};
    };

    enum class StringComparisonPolicy {
        CASE_SENSITIVE,
        CASE_INSENSITIVE
    };

    /**
     * @brief Returns a string which is the concatenation of the string.
     * representation of the given values. Use ConcatStr, methods with *_H are
     * designated as helpers for internal usage.
     * @see ConcatStr(...)
     * */
    template<typename... Args>
    static auto Concat( Args&&... args ) -> std::string {
        fmt::memory_buffer buffer{};
        ( fmt::format_to( std::back_inserter( buffer ),
                          "{}", std::forward<Args>( args ) ),
          ... );

        return fmt::to_string( buffer );
    }

    /**
     * @brief Remove any leading white spaces from str, from both ends.
     * @param str string to be trimmed.
     * @returns copy of str without leading whitespaces from both ends.
     * */
    MKT_NODISCARD inline auto Trim( std::string_view str ) -> std::string {
        auto leftCharIt{ str.begin() };
        auto rightCharIt{ str.rbegin() };

        while ( leftCharIt != str.end() ) {
            if ( std::isspace( *leftCharIt ) ) {
                leftCharIt = std::next( leftCharIt );
            } else {
                break;
            }
        }

        while ( rightCharIt != str.rend() ) {
            if ( std::isspace( *rightCharIt ) ) {
                rightCharIt = std::next( rightCharIt );
            } else {
                break;
            }
        }

        const auto right{ rightCharIt.base() };

        return std::string{ str.substr( leftCharIt - str.begin(), right - leftCharIt ) };
    }


    /**
     * @brief Returns true if two character sequences are equal.
     * @param str1 Null-terminated string to compare.
     * @param str2 Null-terminated string to compare.
     * @param policy Whether we want the comparison to be case-sensitive or not
     * @returns True if both strings are the same, false otherwise.
     * */
    MKT_NODISCARD inline auto Equal( const std::string_view str1, const std::string_view str2, StringComparisonPolicy policy = StringComparisonPolicy::CASE_SENSITIVE ) -> bool {
        const auto insensitive{ []( const char a, const char b ) {
            return std::tolower( a ) == std::tolower( b );
        } };

        const auto sensitive{ []( const char a, const char b ) {
            return a == b;
        } };

        const std::function predicate{
            policy == StringComparisonPolicy::CASE_SENSITIVE ? sensitive : insensitive
        };

        return std::ranges::equal( str1, str2, predicate );
    }

    template<typename CharType>
    inline constexpr auto ReplaceWith( std::string& str, CharType oldVal, CharType newVal ) -> void {
        std::replace( str.begin(), str.end(), oldVal, newVal );
    }


    template<typename... Args>
    auto ConcatenatePath( const Path& first, Args&&... routes ) -> Path {
        Path result{};
        result = result / first;

        std::filesystem::path expansion{};
        if constexpr ( sizeof...( routes ) )
            expansion = std::move( ConcatenatePath( routes... ) );

        result = result / expansion;

        return result;
    }

    template<typename... Args>
    auto ToString( Args&&... args ) -> decltype( auto ) {
        return fmt::to_string( std::forward<Args>( args )... );
    }

    MKT_NODISCARD inline auto ToHex( Size value, const bool upper = true ) -> std::string {
        std::string result{ std::format( "0x{:x}", value ) };

        if ( upper ) {
            std::ranges::transform( result, result.begin(), ::toupper );
        }

        return result;
    }

    MKT_NODISCARD inline auto IsLineFeed( const char value ) -> bool {
        return value == '\n';
    }

    MKT_NODISCARD inline auto IsSpace( const char value ) -> bool {
        return value == ' ';
    }

    template<typename... Args>
    MKT_NODISCARD std::string Format(fmt::format_string<Args...> fmt, Args&&... args) {
        return fmt::format(fmt, std::forward<Args>(args)...);
    }

    MKT_NODISCARD inline auto From( const std::string_view fmt) -> std::string {
        return std::string{ fmt.data() };
    }

    MKT_NODISCARD inline auto Contains( std::string_view str, std::string_view lookUpString) -> bool {
        return str.find(lookUpString) != std::string_view::npos;
    }
}


#endif //MIKOTO_STRING_HH