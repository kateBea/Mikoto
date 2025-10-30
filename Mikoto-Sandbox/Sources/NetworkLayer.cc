//
// Created by kate on 10/30/25.
//

#include "NetworkLayer.hh"

#include <Networking/NetworkService.hh>

namespace Mikoto {

    NetworkLayer::NetworkLayer( std::string_view name )
        :ILayer{ name }
    {
    }

    auto NetworkLayer::OnUpdate( float deltaTime ) -> void {
        if (m_Socket->IsConnected()) {
            const std::string_view data{ "Hello world! " };

            // Void cast to avoid warning
            (void)m_Socket->Send( data.data(), data.size() );
        }
    }

    auto NetworkLayer::OnCreate() -> void {
        m_Socket = NetworkService::Get()->CreateNewSocket( SocketType::SOCKET_TCP, "127.0.0.1", 8000 );
    }

    auto NetworkLayer::OnDestroy() -> void {

    }

    auto NetworkLayer::OnEvent( Event &event ) -> void {

    }
}// namespace Mikoto
