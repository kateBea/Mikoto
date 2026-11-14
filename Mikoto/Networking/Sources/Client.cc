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


#include <exception>

#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <Networking/Client.hh>
#include <Networking/Utility.hh>
#include <Networking/NetworkRequest.hh>

namespace mikoto::network {

    struct HttpClient::AsyncRequest final : INetworkRequest, std::enable_shared_from_this<AsyncRequest> {
        using Clock = std::chrono::steady_clock;
        using Tcp = asio::ip::tcp;

        // Context ownership outlives sockets and every queued callback.
        std::shared_ptr<SocketContext> mOwner;
        asio::strand<asio::io_context::executor_type> mStrand;
        Tcp::resolver mResolver;
        Tcp::socket mSocket;
        asio::steady_timer mDeadline;
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        eastl::unique_ptr<asio::ssl::stream<Tcp::socket>> mTlsSocket{};
#endif

        eastl::array<char, 8192> mBuffer{};
        eastl::string mRequest{};
        eastl::string mRaw{};
        Status mStatus{ Status::Connecting };
        Clock::time_point mStarted{ Clock::now() };
        core::usize mMaxBytes{};

        // UI reads this publication slot, never the live transport state.
        std::mutex mPublicationMutex;
        HttpResponse mResponse{};
        eastl::string mError{};
        Status mPublishedStatus{ Status::Connecting };
        core::usize mPublishedBytes{};
        double mPublishedElapsed{};
        bool mCollected{};

        explicit AsyncRequest( std::shared_ptr<SocketContext> owner )
            : mOwner{ eastl::move( owner ) }, mStrand{ asio::make_strand( mOwner->context ) }, mResolver{ mStrand }, mSocket{ mStrand }, mDeadline{ mStrand } {
        }

        auto Publish() -> void {
            std::lock_guard lock{ mPublicationMutex };
            mPublishedStatus = mStatus;
            mPublishedBytes = mRaw.size();
            mPublishedElapsed = std::chrono::duration<double, std::milli>( Clock::now() - mStarted ).count();
        }

        auto CancelRequest() -> void override {
            asio::post( mStrand, [self = shared_from_this()] {
                self->Finish( Status::Cancelled );
            } );
        }

        auto IsPending() const -> bool {
            return mStatus == Status::Connecting || mStatus == Status::Sending || mStatus == Status::Receiving;
        }

        auto Transport() -> Tcp::socket & {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if ( mTlsSocket ) {
                return mTlsSocket->next_layer();
            }
#endif
            return mSocket;
        }

        auto Finish( Status status, eastl::string_view error = {} ) -> void {
            if ( !IsPending() ) {
                return;
            }

            mStatus = status;
            {
                std::lock_guard lock{ mPublicationMutex };
                mError = error;
            }

            Publish();

            // Keep stream objects and buffers alive until their cancelled
            // handlers are drained by the service.
            mResolver.cancel();
            mDeadline.cancel();
            asio::error_code ignored{};
            Transport().close( ignored );
        }

        auto Fail( const asio::error_code &error ) -> void {
            Finish( Status::Failed, error.message().c_str() );
        }

        auto Read() -> void {
            auto complete{ [this, self = shared_from_this()]( const asio::error_code &error, core::usize count ) {
                if ( !IsPending() ) {
                    return;
                }

                if ( count > mMaxBytes - mRaw.size() ) {
                    Finish( Status::Failed, "Response exceeds the configured size limit (headers included)." );
                    return;
                }

                mRaw.append( mBuffer.data(), count );
                Publish();
                // Requests ask for Connection: close. The existing parser
                // validates framing, content length and chunked bodies at EOF.
                if ( error == asio::error::eof ) {
                    mResponse = GetHttpResponse( mRaw );

                    if ( mResponse.mStatus == "0" ) {
                        Finish( Status::Failed, "Server closed with an incomplete or invalid HTTP response." );
                    }
                    else {
                        Finish( Status::Complete );
                    }
                }
                else if ( error ) {
                    Fail( error );
                }
                else {
                    Read();
                }
            } };

#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if ( mTlsSocket ) {
                mTlsSocket->async_read_some( asio::buffer( mBuffer.data(), mBuffer.size() ), eastl::move( complete ) );
                return;
            }
#endif
            mSocket.async_read_some( asio::buffer( mBuffer.data(), mBuffer.size() ), eastl::move( complete ) );
        }

        auto Write() -> void {
            mStatus = Status::Sending;
            Publish();
            auto complete{ [this, self = shared_from_this()]( const asio::error_code &error, core::usize ) {
                if ( !IsPending() ) {
                    return;
                }

                if ( error ) {
                    Fail( error );
                    return;
                }

                mStatus = Status::Receiving;
                Publish();
                Read();
            } };

#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if ( mTlsSocket ) {
                asio::async_write( *mTlsSocket, asio::buffer( mRequest.data(), mRequest.size() ), eastl::move( complete ) );
                return;
            }
#endif
            asio::async_write( mSocket, asio::buffer( mRequest.data(), mRequest.size() ), eastl::move( complete ) );
        }

        auto Start( eastl::string_view host, core::u16 port, SecurityProtocol security,
            eastl::string request, std::chrono::milliseconds timeout, core::usize maxBytes ) -> void {
            if ( !IsPending() || !mOwner->active ) {
                Finish( Status::Cancelled );
                return;
            }

            mMaxBytes = maxBytes;
            mRequest = eastl::move( request );

            if ( mRequest.empty() || timeout.count() <= 0 || maxBytes == 0 ) {
                Finish( Status::Failed, "Invalid URL, request target, timeout or response-size limit." );
                return;
            }

            // Verify both the certificate chain and host name for HTTPS.
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            if ( security == SecurityProtocol::eTls ) {
                mTlsSocket = eastl::make_unique<asio::ssl::stream<Tcp::socket>>( mStrand, mOwner->tls );
                mTlsSocket->set_verify_mode( asio::ssl::verify_peer );
                const eastl::string name{ host };
                mTlsSocket->set_verify_callback( asio::ssl::host_name_verification( name.c_str() ) );

                if ( !SSL_set_tlsext_host_name( mTlsSocket->native_handle(), name.c_str() ) ) {
                    Finish( Status::Failed, "Cannot configure TLS server name." );
                    return;
                }
            }
#else
            if ( security == SecurityProtocol::eTls ) {
                Finish( Status::Failed, "HTTPS requires OpenSSL support." );
                return;
            }
#endif

            // One deadline covers DNS, TCP connection, TLS, sending and receiving.
            mDeadline.expires_at( mStarted + timeout );
            mDeadline.async_wait( [this, self = shared_from_this()]( const asio::error_code &error ) {
                if ( !error ) {
                    Finish( Status::Failed, "Request timed out." );
                }
            } );

            const eastl::string name{ host };
            mResolver.async_resolve( name.c_str(), string::Format( "{}", port ).c_str(),
                [this, self = shared_from_this()]( const asio::error_code &error, Tcp::resolver::results_type endpoints ) {
                if ( !IsPending() ) {
                    return;
                }

                if ( error ) {
                    Fail( error );
                    return;
                }

                asio::async_connect( Transport(), endpoints, [this, self = shared_from_this()]( const asio::error_code &connectError, const Tcp::endpoint & ) {
                    if ( !IsPending() ) {
                        return;
                    }

                    if ( connectError ) {
                        Fail( connectError );
                        return;
                    }

#if defined( MIKOTO_OPENSSL_AVAILABLE )
                    if ( mTlsSocket ) {
                        mTlsSocket->async_handshake( asio::ssl::stream_base::client,
                            [this, self = shared_from_this()]( const asio::error_code &tlsError ) {
                            if ( !IsPending() ) {
                                return;
                            }

                            if ( tlsError ) {
                                Fail( tlsError );
                            }
                            else {
                                Write();
                            }
                        } );
                        return;
                    }
#endif
                    Write();
                } );
            } );
        }
    };

    HttpClient::~HttpClient() {
        Cancel();
    }

    auto HttpClient::GetAsync( eastl::string_view path, std::chrono::milliseconds timeout, core::usize maxResponseBytes ) -> void {
        MKT_BEGIN_PROFILER();

        Cancel();

        mAsyncRequest.reset();
        mResponse = {};
        mError.clear();
        mReceivedBytes = 0;
        mElapsedMilliseconds = 0;
        mStatus = Status::Failed;

        auto *service{ NetworkService::GetPtr() };

        if ( !mIsValid || !service || !service->IsInitialized() ) {
            mError = "Invalid endpoint or NetworkService is not initialized.";
            return;
        }

        // Registration and submission share the shutdown gate so accepted work
        // is always queued before the service drains cancelled operations.
        auto owner{ service->mContext };
        std::lock_guard lock{ owner->mutex };

        if ( !owner->active ) {
            mError = "NetworkService is shutting down.";
            return;
        }

        auto &requests{ owner->requests };

        for ( auto entry{ requests.begin() }; entry != requests.end(); ) {
            entry = entry->expired() ? requests.erase( entry ) : ++entry;
        }

        mAsyncRequest = std::make_shared<AsyncRequest>( owner );
        requests.emplace_back( mAsyncRequest );
        mStatus = Status::Connecting;
        asio::post( mAsyncRequest->mStrand,
            [request = mAsyncRequest, host = mHost, port = mPort, security = mSecurity,
                bytes = BuildRequest( "GET", path, {}, {} ), timeout, maxResponseBytes] {
            try {
                request->Start( host, port, security, bytes, timeout, maxResponseBytes );
            } catch ( const std::exception &error ) {
                request->Finish( Status::Failed, error.what() );
            }
        } );
    }

    auto HttpClient::Poll() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mAsyncRequest ) {
            return;
        }

        std::lock_guard lock{ mAsyncRequest->mPublicationMutex };
        mStatus = mAsyncRequest->mPublishedStatus;
        mReceivedBytes = mAsyncRequest->mPublishedBytes;
        mElapsedMilliseconds = IsPending()
                                   ? std::chrono::duration<double, std::milli>( AsyncRequest::Clock::now() - mAsyncRequest->mStarted ).count()
                                   : mAsyncRequest->mPublishedElapsed;

        // Transfer completion once; no large response copies during frame updates.
        if ( !IsPending() && !mAsyncRequest->mCollected ) {
            mResponse = eastl::move( mAsyncRequest->mResponse );
            mError = eastl::move( mAsyncRequest->mError );
            mAsyncRequest->mCollected = true;
        }
    }

    auto HttpClient::Cancel() -> void {
        MKT_BEGIN_PROFILER();

        if ( mAsyncRequest ) {
            std::lock_guard lock{ mAsyncRequest->mOwner->mutex };

            if ( mAsyncRequest->mOwner->active ) {
                mAsyncRequest->CancelRequest();
            }
        }
    }

    auto HttpClient::IsPending() const -> bool {
        MKT_BEGIN_PROFILER();
        return mStatus == Status::Connecting || mStatus == Status::Sending || mStatus == Status::Receiving;
    }

    auto HttpClient::GetStatus() const -> Status {
        MKT_BEGIN_PROFILER();
        return mStatus;
    }

    auto HttpClient::GetResponse() const -> const HttpResponse & {
        MKT_BEGIN_PROFILER();
        return mResponse;
    }

    auto HttpClient::GetError() const -> const eastl::string & {
        MKT_BEGIN_PROFILER();
        return mError;
    }

    auto HttpClient::GetReceivedBytes() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mReceivedBytes;
    }

    auto HttpClient::GetElapsedMilliseconds() const -> double {
        MKT_BEGIN_PROFILER();
        return mElapsedMilliseconds;
    }

    HttpClient::HttpClient( const eastl::string_view url ) {
        MKT_BEGIN_PROFILER();
        const HttpUrl parsed{ ParseHttpUrl( url ) };

        if ( !parsed.mIsValid || ( parsed.mUseTls && !NetworkService::HasTlsSupport() ) ) {
            return;
        }

        mHost = parsed.mHost;
        mPort = parsed.mPort;
        mDefaultTarget = parsed.mTarget;
        mSecurity = parsed.mUseTls ? SecurityProtocol::eTls : SecurityProtocol::eNone;
        mIsValid = true;
    }

    HttpClient::HttpClient( const eastl::string_view host, const core::u16 port, const SecurityProtocol security )
        : mPort{ port }, mHost{ host }, mSecurity{ security }, mIsValid{ !host.empty() && port != 0 && ( security != SecurityProtocol::eTls || NetworkService::HasTlsSupport() ) } {
    }

    auto HttpClient::IsValid() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsValid;
    }

    auto HttpClient::BuildRequest( const eastl::string_view method, eastl::string_view path, const eastl::string_view body, const eastl::string_view contentType ) const -> eastl::string {
        MKT_BEGIN_PROFILER();

        if ( path.empty() ) {
            path = mDefaultTarget;
        }

        if ( !path.starts_with( "/" ) ) {
            return {};
        }

        eastl::string request{ method };
        request += " ";
        request.append( path.data(), path.size() );
        request += " HTTP/1.1\r\nHost: ";

        if ( mHost.find( ':' ) != eastl::string::npos ) {
            request += "[";
            request += mHost;
            request += "]";
        }
        else {
            request += mHost;
        }

        request += ":";
        request += string::Format( "{}", mPort );
        request += "\r\nUser-Agent: Mikoto-HttpClient/2.0\r\nAccept: */*\r\nConnection: close\r\n";

        if ( !body.empty() ) {
            request += "Content-Length: ";
            request += string::Format( "{}", body.size() );
            request += "\r\n";
            request += "Content-Type: ";
            const eastl::string_view type{ contentType.empty() ? eastl::string_view{ "application/octet-stream" } : contentType };
            request.append( type.data(), type.size() );
            request += "\r\n";
        }
        else if ( !contentType.empty() ) {
            request += "Content-Type: ";
            request.append( contentType.data(), contentType.size() );
            request += "\r\n";
        }

        request += "\r\n";

        if ( !body.empty() ) {
            request.append( body.data(), body.size() );
        }

        return request;
    }

    auto HttpClient::Get( const eastl::string_view path, const eastl::string_view contentType ) -> HttpResponse {
        MKT_BEGIN_PROFILER();
        return SendRawRequest( BuildRequest( "GET", path, {}, contentType ) );
    }

    auto HttpClient::Post( const eastl::string_view path, const eastl::string_view body, const eastl::string_view contentType ) -> HttpResponse {
        MKT_BEGIN_PROFILER();
        return SendRawRequest( BuildRequest( "POST", path, body, contentType ) );
    }

    auto HttpClient::EnsureSocket() -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mIsValid ) {
            return false;
        }

        if ( !mSocket.IsEmpty() && mSocket->IsConnected() ) {
            return true;
        }

        mSocket = NetworkService::Get().CreateSocketSync( SocketType::eTcp, mHost, mPort, mSecurity );
        return !mSocket.IsEmpty() && mSocket->IsConnected();
    }

    auto HttpClient::SendRawRequest( const eastl::string_view request ) -> HttpResponse {
        MKT_BEGIN_PROFILER();

        if ( request.empty() || !EnsureSocket() || !mSocket->SendSync( request ) ) {
            return {};
        }

        eastl::array<char, 8192> buffer{};
        eastl::string raw{};

        for ( ;; ) {
            const core::usize read{ mSocket->ReceiveSync( buffer.data(), buffer.size() ) };

            if ( read == 0 ) {
                break;
            }

            raw.append( buffer.data(), read );
        }

        mSocket->Disconnect();
        mSocket.Reset();
        return GetHttpResponse( raw );
    }
}// namespace mikoto::network
