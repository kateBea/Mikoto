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

#include <string>
#include <charconv>

#include <Core/Profiler.hh>

#include <Networking/HttpResponse.hh>

namespace mikoto::network {

    auto GetHttpResponse( const eastl::string_view raw ) -> HttpResponse {
        MKT_BEGIN_PROFILER();
        HttpResponse response{};
        const core::usize headerEnd{ raw.find( "\r\n\r\n" ) };
        const core::usize lineEnd{ raw.find( "\r\n" ) };

        if ( headerEnd == eastl::string_view::npos || lineEnd == eastl::string_view::npos ) {
            return response;
        }

        const eastl::string_view statusLine{ raw.substr( 0, lineEnd ) };
        const core::usize firstSpace{ statusLine.find( ' ' ) };
        const core::usize secondSpace{ statusLine.find( ' ', firstSpace + 1 ) };

        if ( firstSpace == eastl::string_view::npos ) {
            return response;
        }

        const auto version{ statusLine.substr( 0, firstSpace ) };

        if ( version != "HTTP/1.0" && version != "HTTP/1.1" ) {
            return {};
        }

        response.mStatus = statusLine.substr( firstSpace + 1, secondSpace - firstSpace - 1 );

        if ( response.mStatus.size() != 3 || response.mStatus[0] < '1' || response.mStatus[0] > '5' ||
             response.mStatus[1] < '0' || response.mStatus[1] > '9' || response.mStatus[2] < '0' || response.mStatus[2] > '9' ) {
            return {};
        }

        if ( secondSpace != eastl::string_view::npos ) {
            response.mReason = statusLine.substr( secondSpace + 1 );
        }

        if ( !ParseHttpHeaders( raw.substr( lineEnd + 2, headerEnd - lineEnd + 2 ), response.mHeaders ) ) {
            return {};
        }

        eastl::string_view body{ raw.substr( headerEnd + 4 ) };
        const auto transferEncoding{ GetHttpHeader( response.mHeaders, "transfer-encoding" ) };

        if ( transferEncoding && NormalizeHttpToken( *transferEncoding ) == "chunked" ) {
            response.mBody.clear();
            bool terminated{};

            while ( !body.empty() ) {
                const core::usize chunkLineEnd{ body.find( "\r\n" ) };

                if ( chunkLineEnd == eastl::string_view::npos ) {
                    return {};
                }

                eastl::string chunkSizeText{ body.substr( 0, chunkLineEnd ) };
                const core::usize extension{ chunkSizeText.find( ';' ) };

                if ( extension != eastl::string::npos ) {
                    chunkSizeText.resize( extension );
                }

                core::usize size{};
                const auto [end, error]{
                    std::from_chars( chunkSizeText.data(), chunkSizeText.data() + chunkSizeText.size(), size, 16 )
                };

                if ( error != std::errc{} || end != chunkSizeText.data() + chunkSizeText.size() ) {
                    return {};
                }

                body.remove_prefix( chunkLineEnd + 2 );

                if ( size == 0 ) {
                    HttpHeaders trailers{};

                    if ( !ParseHttpHeaders( body, trailers ) ) {
                        return {};
                    }

                    terminated = true;
                    break;
                }

                if ( size > body.size() || body.size() - size < 2 || body.substr( size, 2 ) != "\r\n" ) {
                    return {};
                }

                response.mBody.append( body.data(), size );
                body.remove_prefix( size + 2 );
            }

            if ( !terminated ) {
                return {};
            }
        }
        else if ( transferEncoding ) {
            return {};// Unsupported transfer coding; do not present encoded bytes as decoded content.
        }
        else if ( const auto length{ GetHttpHeader( response.mHeaders, "content-length" ) } ) {
            core::usize size{};
            const auto [end, error]{
                std::from_chars( length->data(), length->data() + length->size(), size )
            };

            if ( error != std::errc{} || end != length->data() + length->size() || body.size() < size ) {
                return {};
            }

            response.mBody = body.substr( 0, size );
        }
        else {
            response.mBody = body;
        }

        return response;
    }

    auto GetHttpBody( const eastl::string_view raw ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        return GetHttpResponse( raw ).mBody;
    }

    auto SerializeHttpResponse( const HttpResponse& response ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        eastl::string raw{ "HTTP/1.1 " };
        const eastl::string status{ response.mStatus.empty() || response.mStatus == "0" ? "200" : response.mStatus };

        raw += status;
        raw += " ";
        raw += response.mReason.empty() ? "OK" : response.mReason;
        raw += "\r\n";
        bool hasLength{};
        bool hasConnection{};

        for ( const auto& [name, value]: response.mHeaders ) {
            const eastl::string lower{ NormalizeHttpToken( name ) };
            hasLength |= lower == "content-length";
            hasConnection |= lower == "connection";
            raw += name;
            raw += ": ";
            raw += value;
            raw += "\r\n";
        }

        if ( !hasLength ) {
            raw += "Content-Length: ";
            raw += std::to_string( response.mBody.size() ).c_str();
            raw += "\r\n";
        }

        if ( !hasConnection ) {
            raw += "Connection: close\r\n";
        }

        raw += "\r\n";
        raw += response.mBody;
        return raw;
    }

    auto HttpResponse::IsStatus( const eastl::string_view status ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mStatus == status;
    }

    auto HttpResponse::IsStatusOK() const -> bool {
        MKT_BEGIN_PROFILER();
        return mStatus.size() == 3 && mStatus[0] == '2';
    }

}// namespace mikoto::network
