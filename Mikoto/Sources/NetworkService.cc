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

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>
#include <Networking/NetworkService.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {
    NetworkService::NetworkService( const NetworkServiceCreateInfo & ) {}

    auto NetworkService::CreateSocket( SocketType type, const std::string_view hostName, const UInt16 port, SecurityProtocol sp ) -> SocketHandle {
        SocketHandle handle{ SocketHandle::CreateEmpty() };

        switch (type) {
            case SocketType::SOCKET_TCP:
                handle = CreateSocketTcp( hostName, port, false, sp );
                break;
            case SocketType::SOCKET_UDP:
                // Not supported for now
                MKT_CORE_LOGGER_WARN( "NetworkService::CreateSocket - UDP Socket not yet supported" );
                break;
            default:
                break;
        }

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateSocket - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::CreateSocketSync( SocketType type, std::string_view hostName, UInt16 port, SecurityProtocol sp ) -> SocketHandle {
        SocketHandle handle{ SocketHandle::CreateEmpty() };

        switch (type) {
            case SocketType::SOCKET_TCP:
                handle = CreateSocketTcp( hostName, port, true, sp );
                break;
            case SocketType::SOCKET_UDP:
                // Not supported for now
                MKT_CORE_LOGGER_WARN( "NetworkService::CreateSocketSync - UDP Socket not yet supported" );
                break;
            default:
                break;
        }

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateSocketSync - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::CreateSocketHttp( std::string_view hostName, bool wait ) -> SocketHandle {
        SocketHandle handle{};

        constexpr UInt32 httPort{ 80 };
        handle = CreateSocketTcp( hostName, httPort, wait, SecurityProtocol::NONE );

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "CreateSocketHttp::CreateSocketHttp - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::CreateSocketHttps( std::string_view hostName, bool wait ) -> SocketHandle {
        SocketHandle handle{};

        constexpr UInt32 httPort{ 443 };
        handle = CreateSocketTcp( hostName, httPort, wait, SecurityProtocol::TLS );

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateSocketHttps - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkService::CreateSocketTcp( const std::string_view hostName, const UInt16 port, bool wait, SecurityProtocol sp ) -> SocketHandle {
        SocketHandle handle{ SocketHandle::CreateEmpty() };

        switch (sp) {
            case SecurityProtocol::NONE:
                handle = m_TcpSockets.Allocate( m_IoContext, hostName, port, wait );
                break;
            case SecurityProtocol::TLS:
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                handle = m_TcpSockets.Allocate( m_IoContext, m_SslContext, hostName, port, wait );
#else
                MKT_CORE_LOGGER_WARN("NetworkService::CreateSocketTcp - Attempting to create TLS Socket but OpenSSL not available.");
#endif
                break;
        }

        return handle;
    }

    auto NetworkService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing NetworkService..." );

        m_TcpSockets.Init( 10 );

        m_IsInitialized = true;
    }

    auto NetworkService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down NetworkService..." );

        for (auto &socket: m_TcpSockets | std::views::values) {
            // Wait for pending connections to finish
            while ( socket.As<TcpSocket>()->IsConnectionStatus( ConnectionStatus::PENDING ))
                ;
        }

        // Run pending work if any
        m_IoContext.run();

        m_TcpSockets.Shutdown();

        m_IsInitialized = false;
    }

    auto NetworkService::Update( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_IoContext.poll();
    }
}
