//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#include <string>

#include <Networking/Socket.hh>

#include <Logging/Logger.hh>

namespace mikoto::network {

    using asio::ip::tcp;

    TcpSocket::TcpSocket( asio::io_context& context, const eastl::string_view host, const core::u16 port, const bool connectSynchronously )
        : mContext{ context }, mResolver{ context }, mSocket{ context }, mPort{ port }, mHostName{ host }, mConnectSynchronously{ connectSynchronously } {}

#if defined( MIKOTO_OPENSSL_AVAILABLE )
    TcpSocket::TcpSocket( asio::io_context& context, asio::ssl::context& tlsContext, const eastl::string_view host, const core::u16 port, const bool connectSynchronously )
        : mContext{ context }, mResolver{ context }, mSocket{ context }, mTlsContext{ &tlsContext }, mPort{ port }, mHostName{ host }, mSecurity{ SecurityProtocol::eTLS }, mConnectSynchronously{ connectSynchronously } {}
#endif

    TcpSocket::~TcpSocket() {
        Disconnect();
    }

    auto TcpSocket::Initialize() -> void {
        mIsAllocated = true;
        if ( mConnectSynchronously ) {
            ( void )Connect( mHostName, mPort );
        } else {
            ConnectAsync( mHostName, mPort );
        }
    }

    auto TcpSocket::Destroy() -> void {
        Disconnect();
        mIsAllocated = false;
    }

    auto TcpSocket::SetFailure( const asio::error_code& error ) -> void {
        mLastError = error.message().c_str();
        mConnectionStatus = ConnectionStatus::eFailed;
        MKT_CORE_LOGGER_WARN( "Network connection to '{}:{}' failed: {}", mHostName, mPort, mLastError );
    }

    auto TcpSocket::Disconnect() -> void {
        asio::error_code ignored{};
        mResolver.cancel();
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mTlsSocket ) {
            if ( mTlsSocket->next_layer().is_open() ) {
                mTlsSocket->shutdown( ignored );
                mTlsSocket->next_layer().shutdown( tcp::socket::shutdown_both, ignored );
                mTlsSocket->next_layer().close( ignored );
            }
            mTlsSocket.reset();
        }
#endif
        if ( mSocket.is_open() ) {
            mSocket.shutdown( tcp::socket::shutdown_both, ignored );
            mSocket.close( ignored );
        }
        mConnectionStatus = ConnectionStatus::eDisconnected;
    }

    auto TcpSocket::Connect( const eastl::string_view host, const core::u16 port ) -> bool {
        Disconnect();
        mHostName = host;
        mPort = port;
        mLastError.clear();
        mConnectionStatus = ConnectionStatus::ePending;

        asio::error_code error{};
        const auto endpoints{ mResolver.resolve( mHostName.c_str(), std::to_string( mPort ), error ) };
        if ( error ) {
            SetFailure( error );
            return false;
        }

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTLS ) {
            mTlsSocket = eastl::make_unique<asio::ssl::stream<tcp::socket>>( mContext, *mTlsContext );
            mTlsSocket->set_verify_mode( asio::ssl::verify_peer );
            mTlsSocket->set_verify_callback( asio::ssl::host_name_verification( mHostName.c_str() ) );
            if ( !SSL_set_tlsext_host_name( mTlsSocket->native_handle(), mHostName.c_str() ) ) {
                SetFailure( asio::error::operation_not_supported );
                return false;
            }
            asio::connect( mTlsSocket->next_layer(), endpoints, error );
            if ( !error ) {
                mTlsSocket->handshake( asio::ssl::stream_base::client, error );
            }
        } else
#endif
        {
            asio::connect( mSocket, endpoints, error );
        }

        if ( error ) {
            SetFailure( error );
            return false;
        }
        mConnectionStatus = ConnectionStatus::eConnected;
        return true;
    }

    auto TcpSocket::ConnectAsync( const eastl::string_view host, const core::u16 port, ConnectCallback callback ) -> void {
        Disconnect();
        mHostName = host;
        mPort = port;
        mLastError.clear();
        mConnectionStatus = ConnectionStatus::ePending;

        mResolver.async_resolve( mHostName.c_str(), std::to_string( mPort ), [this, callback = eastl::move( callback )]( const asio::error_code& error, tcp::resolver::results_type endpoints ) mutable {
            if ( error ) {
                SetFailure( error );
                if ( callback ) callback( false, mLastError );
                return;
            }
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if ( mSecurity == SecurityProtocol::eTLS ) {
                mTlsSocket = eastl::make_unique<asio::ssl::stream<tcp::socket>>( mContext, *mTlsContext );
                mTlsSocket->set_verify_mode( asio::ssl::verify_peer );
                mTlsSocket->set_verify_callback( asio::ssl::host_name_verification( mHostName.c_str() ) );
                if ( !SSL_set_tlsext_host_name( mTlsSocket->native_handle(), mHostName.c_str() ) ) {
                    SetFailure( asio::error::operation_not_supported );
                    if ( callback ) callback( false, mLastError );
                    return;
                }
                asio::async_connect( mTlsSocket->next_layer(), endpoints, [this, callback = eastl::move( callback )]( const asio::error_code& connectError, const tcp::endpoint& ) mutable {
                    if ( connectError ) {
                        SetFailure( connectError );
                        if ( callback ) callback( false, mLastError );
                        return;
                    }
                    mTlsSocket->async_handshake( asio::ssl::stream_base::client, [this, callback = eastl::move( callback )]( const asio::error_code& handshakeError ) mutable {
                        if ( handshakeError ) SetFailure( handshakeError ); else mConnectionStatus = ConnectionStatus::eConnected;
                        if ( callback ) callback( !handshakeError, mLastError );
                    } );
                } );
                return;
            }
#endif
            asio::async_connect( mSocket, endpoints, [this, callback = eastl::move( callback )]( const asio::error_code& connectError, const tcp::endpoint& ) mutable {
                if ( connectError ) SetFailure( connectError ); else mConnectionStatus = ConnectionStatus::eConnected;
                if ( callback ) callback( !connectError, mLastError );
            } );
        } );
    }

    auto TcpSocket::SendSync( const void* data, const core::usize size ) -> bool {
        if ( !IsConnected() || data == nullptr || size == 0 ) return false;
        asio::error_code error{};
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTLS ) asio::write( *mTlsSocket, asio::buffer( data, size ), error ); else
#endif
        asio::write( mSocket, asio::buffer( data, size ), error );
        if ( error ) { SetFailure( error ); return false; }
        return true;
    }

    auto TcpSocket::ReceiveSync( void* buffer, const core::usize maxSize ) -> core::usize {
        if ( !IsConnected() || buffer == nullptr || maxSize == 0 ) {
            return 0;
        }

        asio::error_code error{};
        core::usize count{};
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTLS ) count = mTlsSocket->read_some( asio::buffer( buffer, maxSize ), error ); else
#endif
        count = mSocket.read_some( asio::buffer( buffer, maxSize ), error );
        if ( error && error != asio::error::eof
#if defined( MIKOTO_OPENSSL_AVAILABLE )
             && error != asio::ssl::error::stream_truncated
#endif
        ) SetFailure( error );
        return count;
    }

    auto TcpSocket::GetHost() const -> const eastl::string& {
        return mHostName;
    }

    auto TcpSocket::GetLastError() const -> const eastl::string& {
        return mLastError;
    }
}