//    Copyright 2025 ケイト
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

// Specify windows target for asio
#define _WIN32_WINDOWS 0x0A00

#include <Core/Exception.hh>
#include <Networking/Socket.hh>

namespace Mikoto {

    TcpSocket::TcpSocket( asio::io_context &ctx, std::string_view address, UInt16 port, bool wait )
        : m_Socket{ ctx }, m_Port{ port }, m_HostName{ address }, m_InitSync{ wait } {
    }

#if defined( MIKOTO_OPENSSL_AVAILABLE )
    TcpSocket::TcpSocket( asio::io_context &ctx, asio::ssl::context &sslContext, const std::string_view address, const UInt16 port, bool wait )
        : m_Socket{ ctx },
          m_SslSocket{
              new asio::ssl::stream<asio::ip::tcp::socket>{
                  asio::ip::tcp::socket{ ctx },
                  sslContext
              } },
          m_Port{ port },
          m_HostName{ address },
          m_IsSsl{ true },
           m_InitSync( wait ) {
    }
#endif

    TcpSocket::~TcpSocket() {
        while (IsConnectionStatus( ConnectionStatus::PENDING )) {
            // If we made an async connection, and it is still pending
            // wait till it completes
        }

        if (m_IsAllocated) {
            Release();
        }
    }

    auto TcpSocket::Disconnect() -> void {
        try {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if (m_IsSsl) {
                m_SslSocket->shutdown();
                delete m_SslSocket;
                m_SslSocket = nullptr;
            }
#endif
            if (!m_IsSsl) {
                m_Socket.close();
            }

        } catch ( const std::exception& e ) {
            MKT_CORE_LOGGER_ERROR( "TcpSocket::Disconnect - Error Disconnect. e.what() {}", e.what() );
        }

        m_ConnectionStatus = ConnectionStatus::DISCONNECTED;
    }

    auto TcpSocket::IsConnected() const -> bool {
        return IsConnectionStatus( ConnectionStatus::CONNECTED );
    }

    auto TcpSocket::Connect( std::string_view address, UInt16 port ) -> bool {
        if ( IsConnectionStatus( ConnectionStatus::CONNECTED ) ) {
            Disconnect();
        }

        m_Port = port;
        m_HostName = address;

        Initialize();
        return IsConnectionStatus( ConnectionStatus::CONNECTED );
    }

    auto TcpSocket::SendSync( const std::string_view data ) -> void {
        SendSync( data.data(), data.size() );
    }

    auto TcpSocket::SendSync( const void *data, const Size size ) -> void {
        try {
            if ( m_IsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::write( *m_SslSocket, asio::buffer( data, size ) );
#else
#endif
            } else {
                asio::write( m_Socket, asio::buffer( data, size ) );
            }

            m_ErrorCode.clear();

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "TcpSocket::SendSync - Send failed: {}", e.what() );
        }
    }

    auto TcpSocket::ReceiveSync( void *buffer, const Size maxSize ) -> Size {
        try {
            if ( m_ErrorCode == asio::error::eof ) {
                return 0;
            }

            if ( m_IsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                return m_SslSocket->read_some( asio::buffer( buffer, maxSize ), m_ErrorCode );
#else
                return 0;
#endif
            } else {
                return m_Socket.read_some( asio::buffer( buffer, maxSize ), m_ErrorCode );
            }

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "TcpSocket::ReceiveSync - Receive failed: {}", e.what() );
            return 0;
        }
    }

    auto TcpSocket::GetHost() const -> const std::string & {
        return m_HostName;
    }

    auto TcpSocket::Initialize() -> void {
        // We will attempt to connect we set the connection as pending
        m_ConnectionStatus = ConnectionStatus::PENDING;

        if (m_InitSync) {
            InitConnectionSync();
        } else {
            InitConnection();
        }
    }

    auto TcpSocket::Release() -> void {
        Disconnect();

        m_IsAllocated = false;
    }

    auto TcpSocket::InitConnection() -> void {
        try {
            if ( m_IsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::ip::tcp::resolver resolver( m_SslSocket->get_executor() );
                const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

                asio::async_connect( m_SslSocket->next_layer(), endpoints,
                                     [this]( std::error_code ec, const asio::ip::tcp::endpoint & ) -> void {
                                         if ( !ec ) {
                                             MKT_CORE_LOGGER_INFO( "SSL Connected successfully to host {}", m_HostName );

                                             // Perform TLS handshake
                                             m_SslSocket->handshake( asio::ssl::stream_base::client );

                                             m_ConnectionStatus = ConnectionStatus::CONNECTED;
                                         } else {
                                             MKT_CORE_LOGGER_ERROR( "SSL connection failed to host {}, message: {}",m_HostName, ec.message() );

                                             m_ConnectionStatus = ConnectionStatus::DISCONNECTED;
                                         }
                                     } );

#endif
            } else {
                asio::ip::tcp::resolver resolver( m_Socket.get_executor() );
                const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

                asio::async_connect( m_Socket, endpoints, [this]( std::error_code ec, const asio::ip::tcp::endpoint & ) {
                    if ( !ec ) {
                        MKT_CORE_LOGGER_INFO( "TCP Connected successfully to host {}", m_HostName );

                        m_ConnectionStatus = ConnectionStatus::CONNECTED;
                    } else {
                        MKT_CORE_LOGGER_ERROR( "TCP connection failed to host{}, message: {}", m_HostName, ec.message() );

                        m_ConnectionStatus = ConnectionStatus::DISCONNECTED;
                    }
                } );
            }

        } catch ( std::exception &e ) {
            m_ConnectionStatus = ConnectionStatus::DISCONNECTED;
            MKT_CORE_LOGGER_ERROR("Failed to initialize asynchronously to {}. Exception: ", m_HostName, e.what() );
        }
    }

    auto TcpSocket::InitConnectionSync() -> void {
        try {
            if ( m_IsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::ip::tcp::resolver resolver( m_SslSocket->get_executor() );
                const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

                asio::connect( m_SslSocket->next_layer(), endpoints);

                m_SslSocket->handshake( asio::ssl::stream_base::client );

                m_ConnectionStatus = ConnectionStatus::CONNECTED;
                MKT_CORE_LOGGER_INFO( "SSL Connected successfully" );

#else
                throw RuntimeException("TcpSocket::InitConnectionSync - Trying to create SSL TCP Socket but OpenSSL is not available");
#endif
            } else {
                asio::ip::tcp::resolver resolver( m_Socket.get_executor() );
                const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

                m_TcpEndpoint = asio::connect( m_Socket, endpoints );

                m_ConnectionStatus = ConnectionStatus::CONNECTED;
                MKT_CORE_LOGGER_INFO( "TCP Connected successfully" );
            }
        } catch (std::exception& e) {
            m_ConnectionStatus = ConnectionStatus::DISCONNECTED;
            MKT_CORE_LOGGER_ERROR("Failed to initialize synchronously to {}. Excep: ", m_HostName, e.what() );
        }
    }
}// namespace Mikoto