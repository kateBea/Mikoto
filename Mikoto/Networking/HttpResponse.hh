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

#ifndef MIKOTO_NETWORKING_HTTP_RESPONSE_HH
#define MIKOTO_NETWORKING_HTTP_RESPONSE_HH

#include <Networking/Utility.hh>

namespace mikoto::network {

    /** @brief Owned status, headers and decoded body of an HTTP response. */
    struct HttpResponse {
        eastl::string mBody{};
        eastl::string mStatus{ "0" };
        eastl::string mReason{};
        HttpHeaders mHeaders{};

        /**
         * @brief Checks whether a response has an HTTP 2xx status.
         * @returns True for a three-character status beginning with 2.
         */
        MKT_NODISCARD auto IsStatusOK() const -> bool;

        /**
         * @brief Compares the response status text with a supplied code.
         * @param status HTTP status code text.
         * @returns True when status text matches.
         */
        MKT_NODISCARD auto IsStatus( eastl::string_view status ) const -> bool;
    };

    /**
     * @brief Parses an HTTP response and decodes supported body framing.
     * @param raw Complete raw HTTP message, including headers and body.
     * @returns Parsed response, or a default response on failure.
     */
    MKT_NODISCARD auto GetHttpResponse( eastl::string_view raw ) -> HttpResponse;

    /**
     * @brief Extracts the decoded body of a raw HTTP response.
     * @param raw Complete raw HTTP message, including headers and body.
     * @returns Body bytes, or empty text on failure.
     */
    MKT_NODISCARD auto GetHttpBody( eastl::string_view raw ) -> eastl::string;

    /**
     * @brief Serializes a response, filling absent length and connection headers.
     * @param response Status, headers and body to serialize.
     * @returns Complete HTTP/1.1 response bytes.
     */
    MKT_NODISCARD auto SerializeHttpResponse( const HttpResponse& response ) -> eastl::string;

}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_HTTP_RESPONSE_HH
