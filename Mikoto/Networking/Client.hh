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


#ifndef MIKOTO_CLIENT_HH
#define MIKOTO_CLIENT_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Types.hh>
#include <Networking/NetworkService.hh>
#include <Networking/NetworkUtilities.hh>

namespace mikoto::network {

    /** @brief Blocking HTTP/1.1 client intended for worker-thread use. */
    class HttpClient final {
    public:
        explicit HttpClient( eastl::string_view url );
        HttpClient( eastl::string_view host, core::u16 port, SecurityProtocol security = SecurityProtocol::eNone );

        MKT_NODISCARD auto Get( eastl::string_view path, eastl::string_view contentType = {} ) -> HttpResponse;
        MKT_NODISCARD auto Post( eastl::string_view path, eastl::string_view body, eastl::string_view contentType = "application/octet-stream" ) -> HttpResponse;
        MKT_NODISCARD auto IsValid() const -> bool;

    private:
        MKT_NODISCARD auto SendRawRequest( eastl::string_view request ) -> HttpResponse;
        MKT_NODISCARD auto EnsureSocket() -> bool;
        MKT_NODISCARD auto BuildRequest( eastl::string_view method, eastl::string_view path, eastl::string_view body, eastl::string_view contentType ) const -> eastl::string;

    private:
        core::u16 mPort{};
        eastl::string mHost{};
        eastl::string mDefaultTarget{ "/" };
        SecurityProtocol mSecurity{ SecurityProtocol::eNone };
        SocketHandle mSocket{};
        bool mValid{};
    };
}

#endif // MIKOTO_CLIENT_HH
