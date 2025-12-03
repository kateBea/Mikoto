//
// Created by kate on 10/29/25.
//

// Specify windows target for asio
#define _WIN32_WINDOWS 0x0A00

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
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto TcpSocket::Disconnect() -> void {
        try {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            m_SslSocket->shutdown();
#endif
            m_Socket.close();

        } catch ( ... ) {
            // ignore errors
        }

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        delete m_SslSocket;
        m_SslSocket = nullptr;
#endif


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
            MKT_CORE_LOGGER_ERROR( "Send failed: {}", e.what() );
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
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            asio::ip::tcp::resolver resolver( m_SslSocket->get_executor() );
            const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

            asio::async_connect( m_SslSocket->next_layer(), endpoints,
                                 [this]( std::error_code ec, const asio::ip::tcp::endpoint & ) -> void {
                                     if ( !ec ) {
                                         m_Connected = true;
                                         MKT_CORE_LOGGER_INFO( "SSL Connected successfully to host {}", m_HostName );

                                         // Perform TLS handshake
                                         m_SslSocket->handshake( asio::ssl::stream_base::client );
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
        try {
            if ( m_IsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::ip::tcp::resolver resolver( m_SslSocket->get_executor() );
                const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

                asio::connect( m_SslSocket->next_layer(), endpoints);

                m_Connected = true;
                MKT_CORE_LOGGER_INFO( "SSL Connected successfully" );

                // Perform TLS handshake
                m_SslSocket->handshake( asio::ssl::stream_base::client );

#endif
            } else {
                asio::ip::tcp::resolver resolver( m_Socket.get_executor() );
                const auto endpoints{ resolver.resolve( m_HostName, std::to_string( m_Port ) ) };

                m_Connected = true;
                m_TcpEndpoint = asio::connect( m_Socket, endpoints );
                MKT_CORE_LOGGER_INFO( "TCP Connected successfully" );
            }
        } catch (std::exception& e) {
            m_Connected = false;
            MKT_CORE_LOGGER_ERROR("Failed to initialize synchronously to {}. Excep: ", m_HostName, e.what() );
        }
    }
}// namespace Mikoto