//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#include <string>
#include <algorithm>

#include <EASTL/utility.h>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Networking/TcpSocket.hh>

namespace mikoto::network {

    using asio::ip::tcp;

    TcpSocket::TcpSocket( std::shared_ptr<SocketContext> context, const SecurityProtocol security )
        : mContextOwner{ eastl::move( context ) }, mContext{ mContextOwner->context }, mResolver{ mContext }, mSocket{ mContext }, mSecurity{ security } {
        MKT_BEGIN_PROFILER();
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        mTlsContext = &mContextOwner->tls;
#endif
        std::lock_guard lock{ mContextOwner->mutex };
        mContextOwner->sockets.push_back( this );
    }

    TcpSocket::~TcpSocket() {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mContextOwner->mutex };
        Disconnect();
        eastl::erase( mContextOwner->sockets, this );
    }

    auto TcpSocket::SetFailure( const asio::error_code& error ) -> void {
        MKT_BEGIN_PROFILER();
        mLastError = error.message().c_str();
        mConnectionStatus = ConnectionStatus::eFailed;
        MKT_CORE_LOGGER_WARN( "Network connection to '{}:{}' failed: {}", mHostName, mPort, mLastError );
    }

    auto TcpSocket::Disconnect() -> void {
        MKT_BEGIN_PROFILER();
        ++mConnectGeneration;
        asio::error_code ignored{};
        mResolver.cancel();
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mTlsSocket ) {
            if ( mTlsSocket->next_layer().is_open() ) {
                // Closing also cancels a pending handshake; do not block waiting
                // for the peer's TLS close_notify during destruction/shutdown.
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
        MKT_BEGIN_PROFILER();

        Disconnect();

        if ( !mContextOwner->active ) {
            return false;
        }
#if !defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTls ) {
            return false;
        }
#endif
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
        if ( mSecurity == SecurityProtocol::eTls ) {
            mTlsSocket = std::make_shared<asio::ssl::stream<tcp::socket>>( mContext, *mTlsContext );
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
        }
        else
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
        MKT_BEGIN_PROFILER();
        Disconnect();

        if ( !mContextOwner->active ) {
            if ( callback ) {
                callback( false, "Network context is shut down" );
            }

            return;
        }
#if !defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTls ) {
            if ( callback ) {
                callback( false, "OpenSSL is unavailable" );
            }

            return;
        }
#endif
        mHostName = host;
        mPort = port;
        mLastError.clear();
        mConnectionStatus = ConnectionStatus::ePending;

        // Each operation retains the socket until its completion. Cancellation
        // invalidates old completions, including ones already queued by Asio.
        mResolver.async_resolve( mHostName.c_str(), std::to_string( mPort ), [this, self = core::Ref<TcpSocket>{ this }, generation = mConnectGeneration.load(), callback = eastl::move( callback )]( const asio::error_code& error, tcp::resolver::results_type endpoints ) mutable {
            if ( generation != mConnectGeneration || !mContextOwner->active ) {
                return;
            }

            if ( error ) {
                SetFailure( error );

                if ( callback ) {
                    callback( false, mLastError );
                }

                return;
            }
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if ( mSecurity == SecurityProtocol::eTls ) {
                mTlsSocket = std::make_shared<asio::ssl::stream<tcp::socket>>( mContext, *mTlsContext );
                mTlsSocket->set_verify_mode( asio::ssl::verify_peer );
                mTlsSocket->set_verify_callback( asio::ssl::host_name_verification( mHostName.c_str() ) );

                if ( !SSL_set_tlsext_host_name( mTlsSocket->native_handle(), mHostName.c_str() ) ) {
                    SetFailure( asio::error::operation_not_supported );

                    if ( callback ) {
                        callback( false, mLastError );
                    }

                    return;
                }

                asio::async_connect( mTlsSocket->next_layer(), endpoints, [this, self, generation, stream = mTlsSocket, callback = eastl::move( callback )]( const asio::error_code& connectError, const tcp::endpoint& ) mutable {
                    if ( generation != mConnectGeneration || !mContextOwner->active ) {
                        return;
                    }

                    if ( connectError ) {
                        SetFailure( connectError );

                        if ( callback ) {
                            callback( false, mLastError );
                        }

                        return;
                    }

                    stream->async_handshake( asio::ssl::stream_base::client, [this, self, generation, stream, callback = eastl::move( callback )]( const asio::error_code& handshakeError ) mutable {
                        if ( generation != mConnectGeneration || !mContextOwner->active ) {
                            return;
                        }

                        if ( handshakeError ) {
                            SetFailure( handshakeError );
                        }
                        else {
                            mConnectionStatus = ConnectionStatus::eConnected;
                        }

                        if ( callback ) {
                            callback( !handshakeError, mLastError );
                        }
                    } );
                } );
                return;
            }
#endif
            asio::async_connect( mSocket, endpoints, [this, self, generation, callback = eastl::move( callback )]( const asio::error_code& connectError, const tcp::endpoint& ) mutable {
                if ( generation != mConnectGeneration || !mContextOwner->active ) {
                    return;
                }

                if ( connectError ) {
                    SetFailure( connectError );
                }
                else {
                    mConnectionStatus = ConnectionStatus::eConnected;
                }

                if ( callback ) {
                    callback( !connectError, mLastError );
                }
            } );
        } );
    }

    auto TcpSocket::SendSync( const void* data, const core::usize size ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !IsConnected() || data == nullptr || size == 0 ) {
            return false;
        }

        asio::error_code error{};
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTls ) {
            asio::write( *mTlsSocket, asio::buffer( data, size ), error );
        }
        else
#endif
        {
            asio::write( mSocket, asio::buffer( data, size ), error );
        }

        if ( error ) {
            SetFailure( error );
            return false;
        }

        return true;
    }

    auto TcpSocket::ReceiveSync( void* buffer, const core::usize maxSize ) -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( !IsConnected() || buffer == nullptr || maxSize == 0 ) {
            return 0;
        }

        asio::error_code error{};
        core::usize count{};
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        if ( mSecurity == SecurityProtocol::eTls ) {
            count = mTlsSocket->read_some( asio::buffer( buffer, maxSize ), error );
        }
        else
#endif
        {
            if ( mSocket.available() != 0 ) {
                count = mSocket.read_some( asio::buffer( buffer, maxSize ), error );
            }
        }

        if ( error && error != asio::error::eof
#if defined( MIKOTO_OPENSSL_AVAILABLE )
             && error != asio::ssl::error::stream_truncated
#endif
        ) {
            SetFailure( error );
        }

        return count;
    }

    auto TcpSocket::GetHost() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mHostName;
    }

    auto TcpSocket::GetLastError() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mLastError;
    }


    auto TcpSocket::SendSync( const eastl::string_view data ) -> bool {
        MKT_BEGIN_PROFILER();
        return SendSync( data.data(), data.size() );
    }


    auto TcpSocket::GetConnectionStatus() const -> ConnectionStatus {
        MKT_BEGIN_PROFILER();
        return mConnectionStatus.load();
    }


    auto TcpSocket::IsConnectionStatus( const ConnectionStatus status ) const -> bool {
        MKT_BEGIN_PROFILER();
        return GetConnectionStatus() == status;
    }


    auto TcpSocket::IsConnected() const -> bool {
        MKT_BEGIN_PROFILER();
        return IsConnectionStatus( ConnectionStatus::eConnected );
    }
}
