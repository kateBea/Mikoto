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

#ifndef MIKOTO_NETWORKING_HTTP_REQUEST_HH
#define MIKOTO_NETWORKING_HTTP_REQUEST_HH

#include <Networking/Utility.hh>

namespace mikoto::network {

    /** @brief Owned method, target, headers and body of an HTTP request. */
    struct HttpRequest {
        eastl::string mMethod{};
        eastl::string mPath{};
        eastl::string mVersion{ "HTTP/1.1" };
        eastl::string mBody{};
        HttpHeaders mHeaders{};
    };

    /**
     * @brief Parses a complete raw HTTP request into owned fields.
     * @param raw Complete raw HTTP message, including headers and body.
     * @param[out] request Parsed request fields.
     * @returns True when the request is valid and complete.
     */
    MKT_NODISCARD auto ParseHttpRequest( eastl::string_view raw, HttpRequest& request ) -> bool;

}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_HTTP_REQUEST_HH
