//
// Created by kate on 10/29/25.
//

#include <Logging/Logger.hh>
#include <Networking/NetworkService.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {
    NetworkService::NetworkService( const NetworkServiceCreateInfo& ) {
    }

    auto NetworkService::CreateNewSocket( SocketType type, const std::string_view hostName, const UInt16 port, bool allowHttps ) -> SocketHandle {
        // SocketType::SOCKET_TCP ====================
        SocketHandle handle{};

#if defined( MKT_ALLOW_HTTPS )
        handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, port, allowHttps );
#else
        handle = m_TcpSockets.Allocate( m_IoContext, hostName, port );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        }

        return handle;
    }

    auto NetworkService::CreateNewSocketSync( SocketType type, std::string_view hostName, UInt16 port, bool allowHttps ) -> SocketHandle {
        // SocketType::SOCKET_TCP ====================
        SocketHandle handle{};

#if defined( MKT_ALLOW_HTTPS )
        handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, port, allowHttps, true );
#else
        handle = m_TcpSockets.Allocate( m_IoContext, hostName, port );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        }

        return handle;
    }

    auto NetworkService::CreateSocketForHTTPSync( std::string_view hostName ) -> SocketHandle {
        SocketHandle handle{};

#if !defined( MKT_ALLOW_HTTPS )
        handle = m_TcpSockets.Allocate( m_IoContext, hostName, 80, false );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        }

        return handle;
    }

    auto NetworkService::CreateSocketForHTTPSSync( std::string_view hostName ) -> SocketHandle {
        SocketHandle handle{};

#if defined( MKT_ALLOW_HTTPS )
        handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, 443, true );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        }

        return handle;
    }
    auto NetworkService::CreateSocketForHTTPAsync( std::string_view hostName ) -> SocketHandle {
        return SocketHandle::CreateEmpty();
    }

    auto NetworkService::CreateSocketForHTTPSAsync( std::string_view hostName ) -> SocketHandle {
        return SocketHandle::CreateEmpty();
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
