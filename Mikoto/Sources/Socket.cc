//
// Created by kate on 10/29/25.
//


#include <Networking/Socket.hh>

namespace Mikoto {

#if !defined( MKT_ALLOW_HTTPS )
    TcpSocket::TcpSocket( asio::io_context &ctx, std::string_view address, UInt16 port )
        : m_Socket{ ctx }, m_Port{ port }, m_HostName{ address } {
        Initialize();
    }
#else

    TcpSocket::TcpSocket( asio::io_context &ctx, asio::ssl::context &sslContext, const std::string_view address, const UInt16 port, bool ssl )
        : m_Socket{ ctx },
          m_SslSocket{ std::move( asio::ip::tcp::socket{ ctx } ), sslContext },
          m_Port{ port },
          m_HostName{ address },
          m_IsSsl{ ssl } {
        Initialize();
    }

    TcpSocket::TcpSocket( asio::io_context &ctx, asio::ssl::context &sslContext, const std::string_view address, const UInt16 port, bool ssl, bool sync )
        : m_Socket{ ctx },
          m_SslSocket{ std::move( asio::ip::tcp::socket{ ctx } ), sslContext },
          m_Port{ port },
          m_HostName{ address },
          m_IsSsl{ ssl },
           m_InitSync( sync ) {
        Initialize();
    }
#endif


    TcpSocket::~TcpSocket() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto TcpSocket::Disconnect() -> void {
        try {
#if defined( MKT_ALLOW_HTTPS )
            m_SslSocket.shutdown();
#endif
            m_Socket.close();

        } catch ( ... ) {
            // ignore errors
        }

        m_Connected = false;
    }

    auto TcpSocket::IsConnected() const -> bool {
        return m_Connected;
    }

    auto TcpSocket::Connect( std::string_view address, UInt16 port ) -> bool {
        if ( m_Connected ) {
            Disconnect();
        }

        m_Port = port;
        m_HostName = address;

        Initialize();
        return m_Connected;
    }

    auto TcpSocket::SendSync( const std::string_view data ) -> bool {
        return SendSync( data.data(), data.size() );
    }

    auto TcpSocket::SendSync( const void *data, const Size size ) -> bool {
        try {
            if ( m_IsSsl ) {
#if defined( MKT_ALLOW_HTTPS )
                asio::write( m_SslSocket, asio::buffer( data, size ) );
#else
                return false;
#endif
            } else {
                asio::write( m_Socket, asio::buffer( data, size ) );
            }

            m_ErrorCode.clear();

            return true;
        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "Send failed: {}", e.what() );
            return false;
        }
    }

    auto TcpSocket::ReceiveSync( void *buffer, const Size maxSize ) -> Size {
        try {
            if ( m_ErrorCode == asio::error::eof ) {
                return 0;
            }

            if ( m_IsSsl ) {
#if defined( MKT_ALLOW_HTTPS )
                return m_SslSocket.read_some( asio::buffer( buffer, maxSize ), m_ErrorCode );
#else
                return 0;
#endif
            } else {
                return m_Socket.read_some( asio::buffer( buffer, maxSize ), m_ErrorCode );
            }

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "Receive failed: {}", e.what() );
            return 0;
        }
    }

    auto TcpSocket::GetHost() const -> const std::string & {
        return m_HostName;
    }

    auto TcpSocket::Initialize() -> void {
        if (m_InitSync) {
            InitConnectionSync();
            return;
        }

        if ( m_IsSsl ) {
#if defined( MKT_ALLOW_HTTPS )
            asio::ip::tcp::resolver resolver( m_SslSocket.get_executor() );
            const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

            asio::async_connect( m_SslSocket.next_layer(), endpoints,
                                 [this]( std::error_code ec, const asio::ip::tcp::endpoint & ) -> void {
                                     if ( !ec ) {
                                         m_Connected = true;
                                         MKT_CORE_LOGGER_INFO( "SSL Connected successfully to host {}", m_HostName );

                                         // Perform TLS handshake
                                         m_SslSocket.handshake( asio::ssl::stream_base::client );
                                     } else {
                                         MKT_CORE_LOGGER_ERROR( "SSL connection failed to host {}, message: {}",m_HostName, ec.message() );
                                     }
                                 } );

#endif
        } else {
            asio::ip::tcp::resolver resolver( m_Socket.get_executor() );
            const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

            asio::async_connect( m_Socket, endpoints, [this]( std::error_code ec, const asio::ip::tcp::endpoint & ) {
                if ( !ec ) {
                    m_Connected = true;
                    MKT_CORE_LOGGER_INFO( "TCP Connected successfully to host {}", m_HostName );
                } else {
                    MKT_CORE_LOGGER_ERROR( "TCP connection failed to host{}, message: {}", m_HostName, ec.message() );
                }
            } );
        }
    }

    auto TcpSocket::Release() -> void {
        if ( m_Connected ) {
            Disconnect();
        }
        m_IsAllocated = false;
    }

    auto TcpSocket::InitConnectionSync() -> void {
        if ( m_IsSsl ) {
#if defined( MKT_ALLOW_HTTPS )
            asio::ip::tcp::resolver resolver( m_SslSocket.get_executor() );
            const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

            asio::connect( m_SslSocket.next_layer(), endpoints);

            m_Connected = true;
            MKT_CORE_LOGGER_INFO( "SSL Connected successfully" );

            // Perform TLS handshake
            m_SslSocket.handshake( asio::ssl::stream_base::client );

#endif
        } else {
            asio::ip::tcp::resolver resolver( m_Socket.get_executor() );
            const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

            asio::connect( m_Socket, endpoints );

            m_Connected = true;
            MKT_CORE_LOGGER_INFO( "TCP Connected successfully" );
        }
    }
}// namespace Mikoto