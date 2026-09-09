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

#ifndef MIKOTO_NETWORK_UTILITIES_HH
#define MIKOTO_NETWORK_UTILITIES_HH

#include <EASTL/hash_map.h>
#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::network {

    using HttpHeaders = eastl::hash_map<eastl::string, eastl::string>;

    struct HttpRequest {
        eastl::string mMethod{};
        eastl::string mPath{};
        eastl::string mVersion{ "HTTP/1.1" };
        eastl::string mBody{};
        HttpHeaders mHeaders{};
    };

    struct HttpResponse {
        eastl::string mBody{};
        eastl::string mStatus{ "0" };
        eastl::string mReason{};
        HttpHeaders mHeaders{};

        MKT_NODISCARD auto IsStatusOK() const -> bool;
        MKT_NODISCARD auto IsStatus( eastl::string_view status ) const -> bool;
    };

    struct HttpUrl {
        eastl::string mHost{};
        eastl::string mTarget{ "/" };
        core::u16 mPort{};
        bool mUseTls{};
        bool mIsValid{};
    };

    MKT_NODISCARD auto ParseHttpUrl( eastl::string_view url ) -> HttpUrl;
    MKT_NODISCARD auto ParseHttpRequest( eastl::string_view raw, HttpRequest& request ) -> bool;
    MKT_NODISCARD auto GetHttpResponse( eastl::string_view raw ) -> HttpResponse;
    MKT_NODISCARD auto GetHttpBody( eastl::string_view raw ) -> eastl::string;
    MKT_NODISCARD auto SerializeHttpResponse( const HttpResponse& response ) -> eastl::string;
    MKT_NODISCARD auto GetHttpHeader( const HttpHeaders& headers, eastl::string_view name ) -> eastl::optional<eastl::string>;
    MKT_NODISCARD auto GetHost( eastl::string_view uri ) -> eastl::pair<eastl::string, eastl::optional<eastl::string>>;
}

#endif // MIKOTO_NETWORK_UTILITIES_HH
