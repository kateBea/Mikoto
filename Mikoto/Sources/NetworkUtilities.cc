//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#include <Networking/NetworkUtilities.hh>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <string>

namespace mikoto::network {

    // https://www.w3.org/Protocols/HTTP/1.1/rfc2616.pdf

    namespace {
        auto Lower( eastl::string_view value ) -> eastl::string {
            eastl::string result{ value };
            for ( char& character: result ) {
                character = static_cast<char>( std::tolower( static_cast<unsigned char>( character ) ) );
            }
            return result;
        }

        auto Trim( eastl::string_view value ) -> eastl::string_view {
            const auto first{ value.find_first_not_of( " \t\r\n" ) };
            if ( first == eastl::string_view::npos ) {
                return {};
            }
            return value.substr( first, value.find_last_not_of( " \t\r\n" ) - first + 1 );
        }

        auto ParseHeaders( eastl::string_view raw, HttpHeaders& headers ) -> bool {
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

                headers[Lower( Trim( line.substr( 0, colon ) ) )] = eastl::string{ Trim( line.substr( colon + 1 ) ) };
                raw.remove_prefix( end + 2 );
            }
            return false;
        }
    }// namespace

    auto ParseHttpUrl( eastl::string_view url ) -> HttpUrl {
        HttpUrl result{};
        // Request targets and Host headers must not contain whitespace or
        // control bytes. Callers can percent-encode spaces in a filename.
        for ( const unsigned char character: url ) {
            if ( character <= 32 || character == 127 ) return result;
        }

        result.mPort = 80;
        if ( url.starts_with( "https://" ) ) {
            result.mUseTls = true;
            result.mPort = 443;
            url.remove_prefix( 8 );
        } else if ( url.starts_with( "http://" ) )
            url.remove_prefix( 7 );
        else if ( url.find( "://" ) != eastl::string_view::npos )
            return result;

        // Fragments are client-side only. A query without an explicit path
        // still needs the origin-form target "/?query".
        url = url.substr( 0, url.find( '#' ) );
        const core::usize pathStart{ url.find_first_of( "/?" ) };
        const eastl::string_view authority{ url.substr( 0, pathStart ) };
        if ( pathStart != eastl::string_view::npos ) {
            result.mTarget = url[pathStart] == '?' ? "/" : "";
            result.mTarget.append( url.data() + pathStart, url.size() - pathStart );
        }
        if ( authority.empty() || authority.find( '@' ) != eastl::string_view::npos ) return result;

        eastl::string_view portText{};
        bool hasPort{};

        if ( authority.starts_with( "[" ) ) {
            const core::usize closing{ authority.find( ']' ) };
            if ( closing == eastl::string_view::npos ) return result;
            result.mHost = authority.substr( 1, closing - 1 );
            if ( closing + 1 < authority.size() ) {
                if ( authority[closing + 1] != ':' ) return {};
                portText = authority.substr( closing + 2 );
                hasPort = true;
            }
        } else {
            const core::usize colon{ authority.rfind( ':' ) };
            result.mHost = colon == eastl::string_view::npos ? authority : authority.substr( 0, colon );
            if ( colon != eastl::string_view::npos ) {
                if ( result.mHost.find( ':' ) != eastl::string::npos ) return {};
                portText = authority.substr( colon + 1 );
                hasPort = true;
            }
        }
        if ( hasPort ) {
            if ( portText.empty() ) return {};
            unsigned port{};
            const auto [end, error]{ std::from_chars( portText.data(), portText.data() + portText.size(), port ) };
            if ( error != std::errc{} || end != portText.data() + portText.size() || port == 0 || port > 65535 ) return {};
            result.mPort = core::as<core::u16>( port );
        }
        result.mIsValid = !result.mHost.empty();
        return result;
    }

    auto ParseHttpRequest( const eastl::string_view raw, HttpRequest& request ) -> bool {
        const core::usize headerEnd{ raw.find( "\r\n\r\n" ) };
        const core::usize lineEnd{ raw.find( "\r\n" ) };

        if ( headerEnd == eastl::string_view::npos || lineEnd == eastl::string_view::npos ) {
            return false;
        }

        const eastl::string_view startLine{ raw.substr( 0, lineEnd ) };
        const core::usize firstSpace{ startLine.find( ' ' ) };
        const core::usize secondSpace{ startLine.find( ' ', firstSpace + 1 ) };

        if ( firstSpace == eastl::string_view::npos || secondSpace == eastl::string_view::npos ) {
            return false;
        }

        request = {};
        request.mMethod = startLine.substr( 0, firstSpace );
        request.mPath = startLine.substr( firstSpace + 1, secondSpace - firstSpace - 1 );
        request.mVersion = startLine.substr( secondSpace + 1 );

        if ( !ParseHeaders( raw.substr( lineEnd + 2, headerEnd - lineEnd + 2 ), request.mHeaders ) ) {
            return false;
        }

        request.mBody = raw.substr( headerEnd + 4 );
        return true;
    }

    auto GetHttpResponse( const eastl::string_view raw ) -> HttpResponse {
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

        if ( !ParseHeaders( raw.substr( lineEnd + 2, headerEnd - lineEnd + 2 ), response.mHeaders ) ) {
            return {};
        }

        eastl::string_view body{ raw.substr( headerEnd + 4 ) };
        const auto transferEncoding{ GetHttpHeader( response.mHeaders, "transfer-encoding" ) };
        if ( transferEncoding && Lower( *transferEncoding ) == "chunked" ) {
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
                const auto [end, error] {
                    std::from_chars( chunkSizeText.data(), chunkSizeText.data() + chunkSizeText.size(), size, 16 )
                };
                if ( error != std::errc{} || end != chunkSizeText.data() + chunkSizeText.size() ) {
                    return {};
                }

                body.remove_prefix( chunkLineEnd + 2 );
                if ( size == 0 ) {
                    HttpHeaders trailers{};

                    if ( !ParseHeaders( body, trailers ) ) {
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
        } else if ( transferEncoding ) {
            return {};// Unsupported transfer coding; do not present encoded bytes as decoded content.
        } else if ( const auto length{ GetHttpHeader( response.mHeaders, "content-length" ) } ) {
            core::usize size{};
            const auto [end, error] {
                std::from_chars( length->data(), length->data() + length->size(), size )
            };
            if ( error != std::errc{} || end != length->data() + length->size() || body.size() < size ) {
                return {};
            }

            response.mBody = body.substr( 0, size );
        } else
            response.mBody = body;
        return response;
    }

    auto GetHttpBody( const eastl::string_view raw ) -> eastl::string {
        return GetHttpResponse( raw ).mBody;
    }
    auto GetHttpHeader( const HttpHeaders& headers, const eastl::string_view name ) -> eastl::optional<eastl::string> {
        if ( const auto found{ headers.find( Lower( name ) ) }; found != headers.end() ) return found->second;
        return {};
    }

    auto SerializeHttpResponse( const HttpResponse& response ) -> eastl::string {
        eastl::string raw{ "HTTP/1.1 " };
        const eastl::string status{ response.mStatus.empty() || response.mStatus == "0" ? "200" : response.mStatus };

        raw += status;
        raw += " ";
        raw += response.mReason.empty() ? "OK" : response.mReason;
        raw += "\r\n";
        bool hasLength{};
        bool hasConnection{};

        for ( const auto& [name, value]: response.mHeaders ) {
            const eastl::string lower{ Lower( name ) };
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

    auto GetHost( const eastl::string_view uri ) -> eastl::pair<eastl::string, eastl::optional<eastl::string>> {
        const HttpUrl url{ ParseHttpUrl( uri ) };
        if ( !url.mIsValid ) return {};
        return { url.mHost, eastl::string{ std::to_string( url.mPort ).c_str() } };
    }

    auto HttpResponse::IsStatus( const eastl::string_view status ) const -> bool {
        return mStatus == status;
    }

    auto HttpResponse::IsStatusOK() const -> bool {
        return mStatus.size() == 3 && mStatus[0] == '2';
    }
}// namespace mikoto::network
