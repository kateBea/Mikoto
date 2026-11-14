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

#include <cctype>
#include <locale>
#include <codecvt>

#include <EASTL/algorithm.h>
#include <EASTL/functional.h>

#include <Core/String.hh>

namespace mikoto::string {

    auto ConvertToStd( const eastl::string& str ) -> std::string {
        MKT_BEGIN_PROFILER();
        return { str.c_str(), str.size() };
    }

    auto ConvertToEastl( const std::string& str ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        return { str.c_str(), str.size() };
    }

    auto ToLower( char c ) -> char {
        MKT_BEGIN_PROFILER();
        return static_cast<char>( std::tolower( c ) );
    }

    auto FromWChar( const wchar_t* begin, const wchar_t* end ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        eastl::string str( static_cast<long>( eastl::distance( begin, end ) ), '\0' );

        eastl::transform( begin, end, str.begin(),
            []( wchar_t c ) {
            return static_cast<char>( c );
        } );

        return str;
    }

    auto FromWChar( const wchar_t* str ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        return FromWChar( str, str + std::char_traits<wchar_t>::length( str ) );
    }

    auto ToWide( eastl::string_view str ) -> eastl::wstring {
        MKT_BEGIN_PROFILER();
        // DEPRECATED
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter{};
        return converter.from_bytes(
                            str.data(),
                            str.data() + str.size() )
            .c_str();
    }

    auto Trim( eastl::string_view sv ) -> eastl::string_view {
        MKT_BEGIN_PROFILER();

        while ( !sv.empty() && std::isspace( sv.front() ) ) {
            sv.remove_prefix( 1 );
        }
        while ( !sv.empty() && std::isspace( sv.back() ) ) {
            sv.remove_suffix( 1 );
        }

        return sv;
    }

    auto Equal( const eastl::string_view str1, const eastl::string_view str2, ComparePolicy policy ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto insensitive{ []( const char a, const char b ) {
            return std::tolower( a ) == std::tolower( b );
        } };

        const auto sensitive{ []( const char a, const char b ) {
            return a == b;
        } };

        const eastl::function predicate{
            policy == ComparePolicy::eIgnoreCase ? insensitive : sensitive
        };

        return std::ranges::equal( str1, str2, predicate );
    }

    auto IsLineFeed( const char value ) -> bool {
        MKT_BEGIN_PROFILER();
        return value == '\n';
    }

    auto IsNumber( char c ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( c < '0' || c > '9' ) {
            return false;
        }

        return true;
    }

    auto IsNumber( eastl::string_view value ) -> bool {
        MKT_BEGIN_PROFILER();
        return !value.empty() &&
               std::ranges::all_of( value, []( char c ) {
            return IsNumber( c );
        } );
    }

    auto IsSpace( const char value ) -> bool {
        MKT_BEGIN_PROFILER();
        return value == ' ';
    }

    auto From( const eastl::string_view fmt ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        return eastl::string{ fmt.data() };
    }

    auto Contains( std::string_view str, std::string_view lookUpString ) -> bool {
        MKT_BEGIN_PROFILER();
        return str.find( lookUpString ) != std::string_view::npos;
    }

}// namespace mikoto::string
