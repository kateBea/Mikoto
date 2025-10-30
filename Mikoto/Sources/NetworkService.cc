//
// Created by kate on 10/29/25.
//

#include <Logging/Logger.hh>
#include <Networking/NetworkService.hh>

namespace Mikoto {
    NetworkService::NetworkService( const NetworkServiceCreateInfo& ) {
    }

    auto NetworkService::CreateNewSocket( SocketType type, const std::string_view address, const UInt16 port ) -> SocketHandle {
        // SocketType::SOCKET_TCP ====================
        SocketHandle handle{ m_TcpSockets.Allocate( m_IoContext, address, port ) };
        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        }

        return handle;
    }

    auto NetworkService::Init() -> void {

        MKT_CORE_LOGGER_INFO("Initializing NetworkService...");

        m_TcpSockets.Init( 10 );

        m_IsInitialized = true;
    }
    auto NetworkService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down NetworkService..." );

        m_TcpSockets.Shutdown();
    }

    auto NetworkService::Update( float dt ) -> void {
        m_IoContext.poll();
    }
}
