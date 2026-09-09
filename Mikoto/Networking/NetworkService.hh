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

#ifndef MIKOTO_NETWORK_SERVICE_HH
#define MIKOTO_NETWORK_SERVICE_HH

#include <thread>

#include <EASTL/unique_ptr.h>

#include <asio.hpp>
#if defined( MIKOTO_OPENSSL_AVAILABLE )
    #include <asio/ssl.hpp>
#endif

#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>
#include <Core/Types.hh>
#include <Networking/Socket.hh>

namespace mikoto::network {

    enum class SocketType : core::u8 { eTcp, eUdp };

    struct NetworkServiceCreateInfo {
        bool mUseWorkerThread{ true };
        core::u32 mInitialSocketPoolSize{ 16 };
    };

    /**
     * @brief Owns Mikoto's Asio context, socket pool, and optional client TLS context. */
    class NetworkSystem final : public core::ISubsystem, public core::Singleton<NetworkSystem> {
    public:
        explicit NetworkSystem( const NetworkServiceCreateInfo& options );
        ~NetworkSystem() override;
        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float deltaTime ) -> void override;

        MKT_NODISCARD auto CreateSocket( SocketType type, eastl::string_view host, core::u16 port, SecurityProtocol security = SecurityProtocol::eNone ) -> SocketHandle;
        MKT_NODISCARD auto CreateSocketSync( SocketType type, eastl::string_view host, core::u16 port, SecurityProtocol security = SecurityProtocol::eNone ) -> SocketHandle;
        MKT_NODISCARD auto CreateSocketHttp( eastl::string_view host, bool wait = true ) -> SocketHandle;
        MKT_NODISCARD auto CreateSocketHttps( eastl::string_view host, bool wait = true ) -> SocketHandle;
        MKT_NODISCARD auto GetContext() -> asio::io_context&;
        MKT_NODISCARD static constexpr auto HasTlsSupport() -> bool {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            return true;
#else
            return false;
#endif
        }

    private:
        MKT_NODISCARD auto CreateTcpSocket( eastl::string_view host, core::u16 port, bool synchronous, SecurityProtocol security ) -> SocketHandle;

    private:
        NetworkServiceCreateInfo mOptions{};
        asio::io_context mContext{ 1 };
        eastl::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> mWorkGuard{};
        std::jthread mWorker{};
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::ssl::context mTlsContext{ asio::ssl::context::tls_client };
#endif
        core::ResourcePoolTyped<TcpSocket> mTcpSockets{};
    };
}

#endif // MIKOTO_NETWORK_SERVICE_HH
