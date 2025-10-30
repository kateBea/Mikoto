//
// Created by kate on 10/30/25.
//

#include "Networking/NetworkUtilities.hh"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Mikoto::Network {

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

    // Helper: trim whitespace from start and end
    inline std::string_view trim( std::string_view sv ) {
        while ( !sv.empty() && std::isspace( sv.front() ) ) sv.remove_prefix( 1 );
        while ( !sv.empty() && std::isspace( sv.back() ) ) sv.remove_suffix( 1 );
        return sv;
    }

    struct HttpResponse {
        std::string statusLine;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    // Parse raw HTTP response
    auto ParseHttpResponse( std::string_view response ) -> HttpResponse {
        HttpResponse result;

        // Step 1: Read status line
        size_t pos = response.find( "\r\n" );
        if ( pos == std::string_view::npos )
            throw std::runtime_error( "Malformed HTTP response: missing status line" );

        result.statusLine = std::string( response.substr( 0, pos ) );
        response.remove_prefix( pos + 2 );

        // Step 2: Read headers
        while ( true ) {
            size_t headerEnd = response.find( "\r\n" );
            if ( headerEnd == std::string_view::npos )
                throw std::runtime_error( "Malformed HTTP headers" );

            if ( headerEnd == 0 ) {// empty line = end of headers
                response.remove_prefix( 2 );
                break;
            }

            auto line = response.substr( 0, headerEnd );
            response.remove_prefix( headerEnd + 2 );

            size_t colon = line.find( ':' );
            if ( colon == std::string_view::npos )
                throw std::runtime_error( "Malformed HTTP header line" );

            std::string key = std::string( trim( line.substr( 0, colon ) ) );
            std::string value = std::string( trim( line.substr( colon + 1 ) ) );
            result.headers[key] = value;
        }

        // Step 3: Handle body
        auto itTE = result.headers.find( "Transfer-Encoding" );
        auto itCL = result.headers.find( "Content-Length" );

        if ( itTE != result.headers.end() && itTE->second == "chunked" ) {
            // Chunked transfer encoding
            std::string body;
            while ( !response.empty() ) {
                // Read chunk size line
                size_t lineEnd = response.find( "\r\n" );
                if ( lineEnd == std::string_view::npos )
                    throw std::runtime_error( "Malformed chunked encoding" );

                std::string line( response.substr( 0, lineEnd ) );
                response.remove_prefix( lineEnd + 2 );

                size_t chunkSize = std::stoul( line, nullptr, 16 );
                if ( chunkSize == 0 )
                    break;// last chunk

                if ( response.size() < chunkSize + 2 )
                    throw std::runtime_error( "Chunk size exceeds remaining data" );

                body.append( response.substr( 0, chunkSize ) );
                response.remove_prefix( chunkSize + 2 );// skip \r\n after chunk
            }
            result.body = std::move( body );

        } else if ( itCL != result.headers.end() ) {
            // Content-Length present
            size_t contentLength = std::stoul( itCL->second );
            if ( response.size() < contentLength )
                throw std::runtime_error( "Content-Length exceeds remaining data" );

            result.body = std::string( response.substr( 0, contentLength ) );

        } else {
            // Neither Transfer-Encoding nor Content-Length → read till EOF
            result.body = std::string( response );
        }

        return result;
    }


    auto GetBody( std::string_view apiResponse ) -> std::string {
        try {
            auto parsed = ParseHttpResponse( apiResponse );
            return parsed.body;
        } catch ( const std::exception& e ) {
            // Optional: handle parsing errors
            // For now, return empty string on failure
            return "";
        }
    }
}// namespace Mikoto::Network