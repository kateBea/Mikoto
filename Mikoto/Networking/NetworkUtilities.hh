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

#ifndef NETWORK_UTILITIES_HH
#define NETWORK_UTILITIES_HH

#include <EASTL/hash_map.h>
#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

namespace mikoto::network {

    struct HttpRequest {
        eastl::string mMethod{};
        eastl::string mPath{};
        eastl::string mBody{};
        eastl::hash_map<std::string, std::string> mHeaders{};
    };

    struct HttpResponse {
        eastl::string mBody{};
        eastl::string mStatus{ "503" };
        eastl::hash_map<eastl::string, eastl::string> mHeaders{};

        MKT_NODISCARD auto IsStatusOK() const -> bool;
        MKT_NODISCARD auto IsStatus( eastl::string_view status ) const -> bool;
    };


    MKT_NODISCARD auto GetHttpBody( eastl::string_view apiResponse ) -> eastl::string;
    MKT_NODISCARD auto GetHttpResponse( eastl::string_view apiResponse ) -> HttpResponse;

    // Returns the host and the port
    MKT_NODISCARD auto GetHost( eastl::string_view uri ) -> eastl::pair<eastl::string, eastl::optional<eastl::string>>;

}// namespace mikoto::network

#endif
