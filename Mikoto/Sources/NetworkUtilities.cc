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

#include <regex>
#include <optional>
#include <utility>
#include <string>
#include <string_view>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>

#include <Networking/NetworkUtilities.hh>

namespace mikoto::network {

    // Parse raw HTTP response
    auto ParseHttpResponse( eastl::string_view response ) -> HttpResponse {
        HttpResponse result{};

        // Step 1: Read status line
        size_t pos{ response.find( "\r\n" ) };
        if ( pos == eastl::string_view::npos ) {
            MKT_THROW_RUNTIME_ERROR( "Malformed HTTP response: missing status line" );
        }

        result.mStatus = eastl::string( response.substr( 0, pos ) );
        response.remove_prefix( pos + 2 );

        // Step 2: Read headers
        while ( true ) {
            size_t headerEnd{ response.find( "\r\n" ) };
            if ( headerEnd == eastl::string_view::npos ) {
                MKT_THROW_RUNTIME_ERROR( "Malformed HTTP headers" );
            }

            if ( headerEnd == 0 ) { // empty line = end of headers
                response.remove_prefix( 2 );
                break;
            }

            auto line { response.substr( 0, headerEnd ) };
            response.remove_prefix( headerEnd + 2 );

            size_t colon{ line.find( ':' ) };
            if ( colon == eastl::string_view::npos ) {
                MKT_THROW_RUNTIME_ERROR( "Malformed HTTP header line" );
            }

            eastl::string key{ eastl::string( string::Trim( line.substr( 0, colon ) ) ) };
            eastl::string value{ eastl::string( string::Trim( line.substr( colon + 1 ) ) ) };
            result.mHeaders[key] = value;
        }

        // Step 3: Handle body
        auto itTE{ result.mHeaders.find( "Transfer-Encoding" ) };
        auto itCL{ result.mHeaders.find( "Content-Length" ) };

        if ( itTE != result.mHeaders.end() && itTE->second == "chunked" ) {
            // Chunked transfer encoding
            eastl::string body{};
            while ( !response.empty() ) {
                // Read chunk size line
                size_t lineEnd{ response.find( "\r\n" ) };
                if ( lineEnd == eastl::string_view::npos ) {
                    MKT_THROW_RUNTIME_ERROR( "Malformed chunked encoding" );
                }

                eastl::string line( response.substr( 0, lineEnd ) );
                response.remove_prefix( lineEnd + 2 );

                size_t chunkSize{ std::stoul( line.c_str(), nullptr, 16 ) };
                if ( chunkSize == 0 )
                    break;// last chunk

                if ( response.size() < chunkSize + 2 ) {
                    MKT_THROW_RUNTIME_ERROR( "Chunk size exceeds remaining data" );
                }

                body = string::Format( "{}", response.substr( 0, chunkSize ) );
                response.remove_prefix( chunkSize + 2 );// skip \r\n after chunk
            }
            result.mBody = std::move( body );

        } else if ( itCL != result.mHeaders.end() ) {
            // Content-Length present
            size_t contentLength = std::stoul( itCL->second.c_str() );
            if ( response.size() < contentLength ) {
                MKT_THROW_RUNTIME_ERROR( "Content-Length exceeds remaining data" );
            }

            result.mBody = eastl::string( response.substr( 0, contentLength ) );

        } else {
            // Neither Transfer-Encoding nor Content-Length → read till EOF
            result.mBody = eastl::string( response );
        }

        return result;
    }

    auto GetHost( eastl::string_view url ) -> eastl::pair<eastl::string, eastl::optional<eastl::string>> {
        // Regex pattern for URL: scheme://host[:port][/...]
        static const std::regex pattern(R"(^(?:https?:\/\/)?([^\/:]+)(?::(\d+))?.*$)",std::regex::icase);

        std::smatch match{};
        const std::string urlTarget{ url.data() };

        if (std::regex_match(urlTarget, match, pattern)) {
            std::string host{ match[1].str() };
            std::optional<std::string> port{};

            if (match[2].matched) {
                port = match[2].str();
            }

            return { host.c_str(), port->data() };
        }

        // Fallback: not a full URL, just a host
        return eastl::make_pair( urlTarget.c_str(), eastl::optional<eastl::string>() );
    }

    auto GetHttpBody( eastl::string_view apiResponse ) -> eastl::string {
        try {
            auto parsed = ParseHttpResponse( apiResponse );
            return parsed.mBody;
        } catch ( const std::exception& e ) {
            // Optional: handle parsing errors
            // For now, return empty string on failure
            return "";
        }
    }

    auto GetHttpResponse( eastl::string_view apiResponse ) -> HttpResponse {
        try {
            auto parsed = ParseHttpResponse( apiResponse );
            return parsed;
        } catch ( const std::exception& e ) {
            // Optional: handle parsing errors
            // For now, return empty string on failure
            return {};
        }
    }

    auto HttpResponse::IsStatus( eastl::string_view status ) const -> bool {
        return mStatus == status;
    }

    auto HttpResponse::IsStatusOK() const -> bool {
        return mStatus == "200";
    }
}// namespace Mikoto::Network