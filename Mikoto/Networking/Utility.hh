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

#ifndef MIKOTO_NETWORKING_UTILITY_HH
#define MIKOTO_NETWORKING_UTILITY_HH

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/hash_map.h>
#include <EASTL/optional.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::network {

    using HttpHeaders = eastl::hash_map<eastl::string, eastl::string>;

    /**
     * @brief Converts an HTTP token to lower case for case-insensitive comparisons.
     * @param token Header name or token value to normalize.
     * @returns Owned lower-case token text.
     */
    MKT_NODISCARD auto NormalizeHttpToken( eastl::string_view token ) -> eastl::string;

    /**
     * @brief Parses HTTP header lines into normalized names and trimmed values.
     * @param raw Header lines ending with an empty CRLF line.
     * @param[out] headers Receives parsed headers; may contain partial results on failure.
     * @returns True when the terminating empty line is reached.
     */
    MKT_NODISCARD auto ParseHttpHeaders( eastl::string_view raw, HttpHeaders& headers ) -> bool;

    struct HttpUrl {
        eastl::string mHost{};
        eastl::string mTarget{ "/" };
        core::u16 mPort{};
        bool mUseTls{};
        bool mIsValid{};
    };

    /**
     * @brief Parses an HTTP or HTTPS URL into request and connection fields.
     * @param url HTTP or HTTPS URL.
     * @returns Parsed URL; mIsValid reports success.
     */
    MKT_NODISCARD auto ParseHttpUrl( eastl::string_view url ) -> HttpUrl;

    /**
     * @brief Looks up a normalized HTTP header name.
     * @param headers Header map using normalized lower-case keys.
     * @param name Header name to normalize to lower case for lookup.
     * @returns Owned value, or an empty optional when absent.
     */
    MKT_NODISCARD auto GetHttpHeader( const HttpHeaders& headers, eastl::string_view name ) -> eastl::optional<eastl::string>;

    /**
     * @brief Parses a URL's host and effective port.
     * @param uri HTTP or HTTPS URL to parse.
     * @returns Host and decimal port, or an empty pair for invalid input.
     */
    MKT_NODISCARD auto GetHost( eastl::string_view uri ) -> eastl::pair<eastl::string, eastl::optional<eastl::string>>;
}

#endif// MIKOTO_NETWORKING_UTILITY_HH
