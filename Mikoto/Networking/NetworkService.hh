//
// Created by kate on 10/29/25.
//

#ifndef NETWORK_SERVICE_HH
#define NETWORK_SERVICE_HH

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

#include <Networking/Socket.hh>

namespace Mikoto {

    enum class SocketType {
        SOCKET_TCP,
        SOCKET_UDP,
    };

    struct NetworkServiceCreateInfo {
    };

    class NetworkService final : public IService, public Singleton<NetworkService> {
    public:
        explicit NetworkService( const NetworkServiceCreateInfo& options );

        auto CreateNewSocket( SocketType type, std::string_view hostName, UInt16 port, bool allowHttps = false ) -> SocketHandle;
        auto CreateNewSocketSync( SocketType type, std::string_view hostName, UInt16 port, bool allowHttps = false ) -> SocketHandle;

        auto CreateSocketForHTTPSync( std::string_view hostName) -> SocketHandle;
        auto CreateSocketForHTTPSSync( std::string_view hostName) -> SocketHandle;

        auto CreateSocketForHTTPAsync( std::string_view hostName) -> SocketHandle;
        auto CreateSocketForHTTPSAsync( std::string_view hostName) -> SocketHandle;

        ~NetworkService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update( float dt ) -> void override;

    private:
        asio::io_context m_IoContext{};

#if defined(MKT_ALLOW_HTTPS)
        asio::ssl::context m_SslContext{ asio::ssl::context::tls_client };
#endif

        ResourcePoolTyped<TcpSocket> m_TcpSockets{};
    };
}// namespace Mikoto


#endif//
