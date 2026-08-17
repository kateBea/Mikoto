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

#include <string>
#include <exception>

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Platform/PlatformWin32.hh>

#include <asio.hpp>
#if defined( MIKOTO_OPENSSL_AVAILABLE )
#include <asio/ssl.hpp>
#endif

#include <Core/Exception.hh>
#include <Networking/Socket.hh>

namespace mikoto::network {

    using namespace mikoto::core;

    TcpSocket::TcpSocket( asio::io_context &ctx, eastl::string_view address, u16 port, bool wait )
        : mSocket{ ctx }, mPort{ port }, mHostName{ address }, mInitSync{ wait } {
    }

#if defined( MIKOTO_OPENSSL_AVAILABLE )
    TcpSocket::TcpSocket( asio::io_context& ctx, asio::ssl::context& sslContext, eastl::string_view address, core::u16 port, bool wait )
        : mSocket{ ctx },
          mSslSocket{
              new asio::ssl::stream<asio::ip::tcp::socket>{
                  asio::ip::tcp::socket{ ctx },
                  sslContext
              } },
          mPort{ port },
          mHostName{ address },
          mIsSsl{ true },
           mInitSync( wait ) {
    }
#endif

    TcpSocket::~TcpSocket() {
        while (IsConnectionStatus( ConnectionStatus::ePending )) {
            // If we made an async connection, and it is still pending
            // wait till it completes
        }

        if (mIsAllocated) {
            Release();
        }
    }

    auto TcpSocket::Disconnect() -> void {
        try {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if (mIsSsl) {
                mSslSocket->shutdown();
                delete mSslSocket;
                mSslSocket = nullptr;
            }
#endif
            if (!mIsSsl) {
                mSocket.close();
            }

        } catch ( const std::exception& e ) {
            MKT_CORE_LOGGER_ERROR( "TcpSocket::Disconnect - Error Disconnect. e.what() {}", e.what() );
        }

        mConnectionStatus = ConnectionStatus::eDisconnected;
    }

    auto TcpSocket::IsConnected() const -> bool {
        return IsConnectionStatus( ConnectionStatus::eConnected );
    }

    auto TcpSocket::Connect( eastl::string_view address, u16 port ) -> bool {
        if ( IsConnectionStatus( ConnectionStatus::eConnected ) ) {
            Disconnect();
        }

        mPort = port;
        mHostName = address;

        Initialize();
        return IsConnectionStatus( ConnectionStatus::eConnected );
    }

    auto TcpSocket::SendSync( const eastl::string_view data ) -> void {
        SendSync( data.data(), data.size() );
    }

    auto TcpSocket::SendSync( const void *data, const size_t size ) -> void {
        try {
            if ( mIsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::write( *mSslSocket, asio::buffer( data, size ) );
#else
#endif
            } else {
                asio::write( mSocket, asio::buffer( data, size ) );
            }

            mErrorCode.clear();

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "TcpSocket::SendSync - Send failed: {}", e.what() );
        }
    }

    auto TcpSocket::ReceiveSync( void *buffer, const size_t maxSize ) -> size_t {
        try {
            if ( mErrorCode == asio::error::eof ) {
                return 0;
            }

            if ( mIsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                return mSslSocket->read_some( asio::buffer( buffer, maxSize ), mErrorCode );
#else
                return 0;
#endif
            } else {
                return mSocket.read_some( asio::buffer( buffer, maxSize ), mErrorCode );
            }

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "TcpSocket::ReceiveSync - Receive failed: {}", e.what() );
            return 0;
        }
    }

    auto TcpSocket::GetHost() const -> const eastl::string & {
        return mHostName;
    }

    auto TcpSocket::Initialize() -> void {
        // We will attempt to connect we set the connection as pending
        mConnectionStatus = ConnectionStatus::ePending;

        if (mInitSync) {
            InitConnectionSync();
        } else {
            InitConnection();
        }
    }

    auto TcpSocket::Release() -> void {
        Disconnect();

        mIsAllocated = false;
    }

    auto TcpSocket::InitConnection() -> void {
        try {
            if ( mIsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::ip::tcp::resolver resolver( mSslSocket->get_executor() );
                const auto endpoints{ resolver.resolve( mHostName.c_str(), std::to_string( mPort ) ) };

                asio::async_connect( mSslSocket->next_layer(), endpoints,
                     [this]( std::error_code ec, const asio::ip::tcp::endpoint & ) -> void {
                         if ( !ec ) {
                             MKT_CORE_LOGGER_INFO( "SSL Connected successfully to host {}", mHostName );

                             // Perform TLS handshake
                             mSslSocket->handshake( asio::ssl::stream_base::client );

                             mConnectionStatus = ConnectionStatus::eConnected;
                         } else {
                             MKT_CORE_LOGGER_ERROR( "SSL connection failed to host {}, message: {}",mHostName, ec.message() );

                             mConnectionStatus = ConnectionStatus::eDisconnected;
                         }
                     } );

#endif
            } else {
                asio::ip::tcp::resolver resolver( mSocket.get_executor() );
                const auto endpoints{ resolver.resolve( mHostName.c_str(), std::to_string( mPort ) ) };

                asio::async_connect( mSocket, endpoints, [this]
                    ( std::error_code ec, const asio::ip::tcp::endpoint & ) {
                    if ( !ec ) {
                        MKT_CORE_LOGGER_INFO( "TCP Connected successfully to host {}", mHostName );

                        mConnectionStatus = ConnectionStatus::eConnected;
                    } else {
                        MKT_CORE_LOGGER_ERROR( "TCP connection failed to host{}, message: {}", mHostName, ec.message() );

                        mConnectionStatus = ConnectionStatus::eDisconnected;
                    }
                } );
            }

        } catch ( std::exception &e ) {
            mConnectionStatus = ConnectionStatus::eDisconnected;
            MKT_CORE_LOGGER_ERROR("Failed to initialize asynchronously to {}. Exception: ", mHostName, e.what() );
        }
    }

    auto TcpSocket::InitConnectionSync() -> void {
        try {
            if ( mIsSsl ) {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                asio::ip::tcp::resolver resolver( mSslSocket->get_executor() );
                const auto endpoints{ resolver.resolve( mHostName.c_str(), std::to_string( mPort ) ) };

                asio::connect( mSslSocket->next_layer(), endpoints);

                mSslSocket->handshake( asio::ssl::stream_base::client );

                mConnectionStatus = ConnectionStatus::eConnected;
                MKT_CORE_LOGGER_INFO( "SSL Connected successfully" );

#else
                throw RuntimeException("TcpSocket::InitConnectionSync - Trying to create SSL TCP Socket but OpenSSL is not available");
#endif
            } else {
                asio::ip::tcp::resolver resolver( mSocket.get_executor() );
                const auto endpoints{ resolver.resolve( mHostName.c_str(), std::to_string( mPort ) ) };

                mTcpEndpoint = asio::connect( mSocket, endpoints );

                mConnectionStatus = ConnectionStatus::eConnected;
                MKT_CORE_LOGGER_INFO( "TCP Connected successfully" );
            }
        } catch (std::exception& e) {
            mConnectionStatus = ConnectionStatus::eDisconnected;
            MKT_CORE_LOGGER_ERROR("Failed to initialize synchronously to {}. Excep: ", mHostName, e.what() );
        }
    }
}// namespace Mikoto