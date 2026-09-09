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


#include <EASTL/array.h>

#include <Networking/Client.hh>

namespace mikoto::network {

    HttpClient::HttpClient( const eastl::string_view url ) {
        const HttpUrl parsed{ ParseHttpUrl( url ) };
        if ( !parsed.mIsValid || ( parsed.mUseTls && !NetworkSystem::HasTlsSupport() ) ) {
            return;
        }

        mHost = parsed.mHost;
        mPort = parsed.mPort;
        mDefaultTarget = parsed.mTarget;
        mSecurity = parsed.mUseTls ? SecurityProtocol::eTLS : SecurityProtocol::eNone;
        mValid = true;
    }

    HttpClient::HttpClient( const eastl::string_view host, const core::u16 port, const SecurityProtocol security )
        : mPort{ port }, mHost{ host }, mSecurity{ security }, mValid{ !host.empty() && port != 0 && ( security != SecurityProtocol::eTLS || NetworkSystem::HasTlsSupport() ) } {}

    auto HttpClient::IsValid() const -> bool {
        return mValid;
    }

    auto HttpClient::BuildRequest( const eastl::string_view method, eastl::string_view path, const eastl::string_view body, const eastl::string_view contentType ) const -> eastl::string {
        if ( path.empty() ) {
            path = mDefaultTarget;
        }

        if ( !path.starts_with( "/" ) ) {
            return {};
        }

        eastl::string request{ method };
        request += " "; request += path.data(); request += " HTTP/1.1\r\nHost: "; request += mHost;
        request += "\r\nUser-Agent: Mikoto-HttpClient/2.0\r\nAccept: */*\r\nConnection: close\r\n";

        if ( !body.empty() ) {
            request += "Content-Length: "; request += eastl::to_string( body.size() ); request += "\r\n";
            request += "Content-Type: "; request += contentType.empty() ? "application/octet-stream" : contentType.data(); request += "\r\n";
        } else if ( !contentType.empty() ) {
            request += "Content-Type: "; request += contentType.data(); request += "\r\n";
        }

        request += "\r\n"; request += body.data();
        return request;
    }

    auto HttpClient::Get( const eastl::string_view path, const eastl::string_view contentType ) -> HttpResponse {
        return SendRawRequest( BuildRequest( "GET", path, {}, contentType ) );
    }

    auto HttpClient::Post( const eastl::string_view path, const eastl::string_view body, const eastl::string_view contentType ) -> HttpResponse {
        return SendRawRequest( BuildRequest( "POST", path, body, contentType ) );
    }

    auto HttpClient::EnsureSocket() -> bool {
        if ( !mValid ) {
            return false;
        }

        if ( !mSocket.IsEmpty() && mSocket->IsConnected() ) {
            return true;
        }

        mSocket = NetworkSystem::Get().CreateSocketSync( SocketType::eTcp, mHost, mPort, mSecurity );
        return !mSocket.IsEmpty() && mSocket->IsConnected();
    }

    auto HttpClient::SendRawRequest( const eastl::string_view request ) -> HttpResponse {
        if ( request.empty() || !EnsureSocket() || !mSocket->SendSync( request ) ) {
            return {};
        }

        eastl::array<char, 8192> buffer{};
        eastl::string raw{};

        for (;;) {
            const core::usize read{ mSocket->ReceiveSync( buffer.data(), buffer.size() ) };
            if ( read == 0 ) break;
            raw.append( buffer.data(), read );
        }

        mSocket->Disconnect();
        mSocket.Reset();
        return GetHttpResponse( raw );
    }
}
