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
#include <string>
#include <charconv>

#include <Core/Profiler.hh>

#include <Networking/Utility.hh>

namespace mikoto::network {

    // https://www.w3.org/Protocols/HTTP/1.1/rfc2616.pdf

    static auto Trim( eastl::string_view value ) -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        const auto first{ value.find_first_not_of( " \t\r\n" ) };

        if ( first == eastl::string_view::npos ) {
            return {};
        }

        return value.substr( first, value.find_last_not_of( " \t\r\n" ) - first + 1 );
    }


    auto NormalizeHttpToken( eastl::string_view value ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        eastl::string result{ value };

        for ( char& character: result ) {
            character = static_cast<char>( std::tolower( static_cast<unsigned char>( character ) ) );
        }

        return result;
    }


    auto ParseHttpHeaders( eastl::string_view raw, HttpHeaders& headers ) -> bool {
        MKT_BEGIN_PROFILER();

        while ( !raw.empty() ) {
            const core::usize end{ raw.find( "\r\n" ) };
            const eastl::string_view line{ raw.substr( 0, end ) };

            if ( line.empty() ) {
                return true;
            }

            if ( end == eastl::string_view::npos ) {
                return false;
            }

            const core::usize colon{ line.find( ':' ) };

            if ( colon == eastl::string_view::npos ) {
                return false;
            }

            headers[NormalizeHttpToken( Trim( line.substr( 0, colon ) ) )] = eastl::string{ Trim( line.substr( colon + 1 ) ) };
            raw.remove_prefix( end + 2 );
        }

        return false;
    }

    auto ParseHttpUrl( eastl::string_view url ) -> HttpUrl {
        MKT_BEGIN_PROFILER();
        HttpUrl result{};
        // Request targets and Host headers must not contain whitespace or
        // control bytes. Callers can percent-encode spaces in a filename.
        for ( const unsigned char character: url ) {
            if ( character <= 32 || character == 127 ) {
                return result;
            }
        }

        result.mPort = 80;

        if ( url.starts_with( "https://" ) ) {
            result.mUseTls = true;
            result.mPort = 443;
            url.remove_prefix( 8 );
        }
        else if ( url.starts_with( "http://" ) ) {
            url.remove_prefix( 7 );
        }
        else if ( url.find( "://" ) != eastl::string_view::npos ) {
            return result;
        }

        // Fragments are client-side only. A query without an explicit path
        // still needs the origin-form target "/?query".
        url = url.substr( 0, url.find( '#' ) );
        const core::usize pathStart{ url.find_first_of( "/?" ) };
        const eastl::string_view authority{ url.substr( 0, pathStart ) };

        if ( pathStart != eastl::string_view::npos ) {
            result.mTarget = url[pathStart] == '?' ? "/" : "";
            result.mTarget.append( url.data() + pathStart, url.size() - pathStart );
        }

        if ( authority.empty() || authority.find( '@' ) != eastl::string_view::npos ) {
            return result;
        }

        eastl::string_view portText{};
        bool hasPort{};

        if ( authority.starts_with( "[" ) ) {
            const core::usize closing{ authority.find( ']' ) };

            if ( closing == eastl::string_view::npos ) {
                return result;
            }

            result.mHost = authority.substr( 1, closing - 1 );

            if ( closing + 1 < authority.size() ) {
                if ( authority[closing + 1] != ':' ) {
                    return {};
                }

                portText = authority.substr( closing + 2 );
                hasPort = true;
            }
        }
        else {
            const core::usize colon{ authority.rfind( ':' ) };
            result.mHost = colon == eastl::string_view::npos ? authority : authority.substr( 0, colon );

            if ( colon != eastl::string_view::npos ) {
                if ( result.mHost.find( ':' ) != eastl::string::npos ) {
                    return {};
                }

                portText = authority.substr( colon + 1 );
                hasPort = true;
            }
        }

        if ( hasPort ) {
            if ( portText.empty() ) {
                return {};
            }

            unsigned port{};
            const auto [end, error]{ std::from_chars( portText.data(), portText.data() + portText.size(), port ) };

            if ( error != std::errc{} || end != portText.data() + portText.size() || port == 0 || port > 65535 ) {
                return {};
            }

            result.mPort = core::as<core::u16>( port );
        }

        result.mIsValid = !result.mHost.empty();
        return result;
    }

    auto GetHttpHeader( const HttpHeaders& headers, const eastl::string_view name ) -> eastl::optional<eastl::string> {
        MKT_BEGIN_PROFILER();

        if ( const auto found{ headers.find( NormalizeHttpToken( name ) ) }; found != headers.end() ) {
            return found->second;
        }

        return {};
    }

    auto GetHost( const eastl::string_view uri ) -> eastl::pair<eastl::string, eastl::optional<eastl::string>> {
        MKT_BEGIN_PROFILER();
        const HttpUrl url{ ParseHttpUrl( uri ) };

        if ( !url.mIsValid ) {
            return {};
        }

        return { url.mHost, eastl::string{ std::to_string( url.mPort ).c_str() } };
    }

}// namespace mikoto::network
