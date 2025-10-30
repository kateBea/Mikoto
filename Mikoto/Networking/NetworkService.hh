//
// Created by kate on 10/29/25.
//

#ifndef NETWORK_SERVICE_HH
#define NETWORK_SERVICE_HH

#include <asio.hpp>

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

        auto CreateNewSocket( SocketType type, std::string_view address, UInt16 port ) -> SocketHandle;

        ~NetworkService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update( float dt ) -> void override;

    private:
        asio::io_context m_IoContext{};

        ResourcePoolTyped<TcpSocket> m_TcpSockets{};
    };
}// namespace Mikoto


#endif//
