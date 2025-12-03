//
// Created by kate on 10/29/25.
//

#include <tracy/Tracy.hpp>

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>
#include <Networking/NetworkService.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {
    NetworkService::NetworkService( const NetworkServiceCreateInfo& ) {
    }

    auto NetworkService::CreateSocket( SocketType type, const std::string_view hostName, const UInt16 port ) -> SocketHandle {
        // SocketType::SOCKET_TCP ====================
        SocketHandle handle{};

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, port, false );
#else
        handle = m_TcpSockets.Allocate( m_IoContext, hostName, port );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::CreateSocketSync( SocketType type, std::string_view hostName, UInt16 port) -> SocketHandle {
        // SocketType::SOCKET_TCP ====================
        SocketHandle handle{};

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, port, true );
#else
        handle = m_TcpSockets.Allocate( m_IoContext, hostName, port );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }
    auto NetworkService::CreateSocketHttp( std::string_view hostName, bool wait ) -> SocketHandle {
        SocketHandle handle{};

#if !defined( MIKOTO_OPENSSL_AVAILABLE )
        handle = m_TcpSockets.Allocate( m_IoContext, hostName, 80 );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket for HTTP" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::CreateSocketHttps( std::string_view hostName, bool wait ) -> SocketHandle {
        SocketHandle handle{};

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, 443, true );
#endif

        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateNewSocket - Failed to create new socket for HTTPS" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing NetworkService...");

        m_TcpSockets.Init( 10 );

        m_IsInitialized = true;
    }
    auto NetworkService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down NetworkService..." );

        m_TcpSockets.Shutdown();
    }

    auto NetworkService::Update( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_IoContext.poll();
    }
}
