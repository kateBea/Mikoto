//
// Created by kate on 10/29/25.
//

#ifndef NETWORK_SERVICE_HH
#define NETWORK_SERVICE_HH

#include <asio.hpp>

#if defined(MKT_ALLOW_HTTPS)
#include <asio/ssl.hpp>
#endif

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

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update( float dt ) -> void override;

        auto CreateNewSocket( SocketType type, std::string_view hostName, UInt16 port) -> SocketHandle;
        auto CreateNewSocketSync( SocketType type, std::string_view hostName, UInt16 port) -> SocketHandle;

        // wait blocks the calling thread until this socket is fully created and rdy to use
        auto CreateSocketHTTP( std::string_view hostName, bool wait) -> SocketHandle;

        // wait blocks the calling thread until this socket is fully created and rdy to use
        auto CreateSocketHTTPS( std::string_view hostName, bool wait) -> SocketHandle;

        ~NetworkService() override = default;

    private:
        asio::io_context m_IoContext{};

#if defined(MKT_ALLOW_HTTPS)
        asio::ssl::context m_SslContext{ asio::ssl::context::tls_client };
#endif

        ResourcePoolTyped<TcpSocket> m_TcpSockets{};
    };
}// namespace Mikoto


#endif//
