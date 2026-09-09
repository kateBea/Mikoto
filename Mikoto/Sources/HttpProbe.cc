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

#include <array>
#include <string>
#include <vector>

#include <asio.hpp>
#include <Networking/HttpProbe.hh>

namespace mikoto::network {

    struct HttpProbe::Impl {
        using Clock = std::chrono::steady_clock;
        using Tcp = asio::ip::tcp;

        std::vector<Tcp::endpoint> mEndpoints{};
        std::array<char, 8192> mBuffer{};
        eastl::string mUrl{}, mRequest{}, mRaw{}, mError{};
        HttpResponse mResponse{};
        Status mStatus{ Status::Connecting };
        Clock::time_point mStarted{ Clock::now() }, mFinished{};
        core::usize mMaxBytes{};

        // Destroy I/O objects and their context before the backing buffers.
        // Cancelled operations must release those buffers before they are freed.
        // No handler is dispatched except by Poll on the owning thread.
        asio::io_context mContext{};
        Tcp::socket mSocket{ mContext };
        asio::steady_timer mDeadline{ mContext };

        auto IsPending() const -> bool {
            return mStatus == Status::Connecting || mStatus == Status::Sending || mStatus == Status::Receiving;
        }

        auto Finish( const Status status, const eastl::string_view error = {} ) -> void {
            if ( !IsPending() ) return;
            mStatus = status;
            mError = error;
            mFinished = Clock::now();
            mDeadline.cancel();
            asio::error_code ignored{};
            mSocket.close( ignored );
        }

        auto Fail( const char* stage, const asio::error_code& error ) -> void {
            eastl::string message{ stage };
            message += ": ";
            message += error.message().c_str();
            Finish( Status::Failed, message );
        }

        auto Read() -> void {
            mSocket.async_read_some( asio::buffer( mBuffer ), [this]( const asio::error_code& error, const std::size_t count ) {
                if ( !IsPending() ) return;
                if ( count > mMaxBytes - mRaw.size() ) {
                    Finish( Status::Failed, "Response exceeds the configured size limit (headers included)." );
                    return;
                }
                mRaw.append( mBuffer.data(), count );
                if ( error == asio::error::eof ) {
                    mResponse = GetHttpResponse( mRaw );
                    if ( mResponse.mStatus == "0" ) Finish( Status::Failed, "Server closed with an incomplete or invalid HTTP response." );
                    else Finish( Status::Complete );
                } else if ( error ) Fail( "Receive failed", error );
                else Read();
            } );
        }

        explicit Impl( const eastl::string_view url, const std::chrono::milliseconds timeout, const core::usize maxBytes )
            : mUrl{ url }, mMaxBytes{ maxBytes } {
            const HttpUrl parsed{ ParseHttpUrl( url ) };
            if ( !url.starts_with( "http://" ) || !parsed.mIsValid || parsed.mUseTls ) {
                Finish( Status::Failed, "Enter an HTTP URL, for example http://localhost:9000/. HTTPS is not supported by this probe." );
                return;
            }
            if ( timeout.count() <= 0 || maxBytes == 0 ) {
                Finish( Status::Failed, "Timeout and response size limit must be positive." );
                return;
            }

            // Avoid blocking DNS entirely. Python's default bind may be IPv6
            // or IPv4, so localhost tries both loopback addresses.
            if ( parsed.mHost == "localhost" ) {
                mEndpoints.emplace_back( asio::ip::address_v6::loopback(), parsed.mPort );
                mEndpoints.emplace_back( asio::ip::address_v4::loopback(), parsed.mPort );
            } else {
                asio::error_code error{};
                const auto address{ asio::ip::make_address( parsed.mHost.c_str(), error ) };
                if ( error || address.is_unspecified() ) {
                    Finish( Status::Failed, "Use localhost or an IPv4/IPv6 address. Use [::1], not the server's wildcard bind address [::]." );
                    return;
                }
                mEndpoints.emplace_back( address, parsed.mPort );
            }

            mRequest = "GET ";
            mRequest += parsed.mTarget;
            mRequest += " HTTP/1.1\r\nHost: ";
            if ( parsed.mHost.find( ':' ) != eastl::string::npos ) mRequest += "[";
            mRequest += parsed.mHost;
            if ( parsed.mHost.find( ':' ) != eastl::string::npos ) mRequest += "]";
            mRequest += ":";
            mRequest += std::to_string( parsed.mPort ).c_str();
            mRequest += "\r\nUser-Agent: Mikoto-NetworkDebugLayer/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n";

            mDeadline.expires_after( timeout );
            mDeadline.async_wait( [this]( const asio::error_code& error ) {
                if ( !error ) Finish( Status::Failed, "Request timed out before the server closed the response." );
            } );
            asio::async_connect( mSocket, mEndpoints, [this]( const asio::error_code& error, const Tcp::endpoint& ) {
                if ( !IsPending() ) return;
                if ( error ) { Fail( "Connect failed", error ); return; }
                mStatus = Status::Sending;
                asio::async_write( mSocket, asio::buffer( mRequest.data(), mRequest.size() ), [this]( const asio::error_code& writeError, std::size_t ) {
                    if ( !IsPending() ) return;
                    if ( writeError ) { Fail( "Send failed", writeError ); return; }
                    mStatus = Status::Receiving;
                    Read();
                } );
            } );
        }
    };

    HttpProbe::HttpProbe( const eastl::string_view url, const std::chrono::milliseconds timeout, const core::usize maxResponseBytes )
        : mImpl{ std::make_unique<Impl>( url, timeout, maxResponseBytes ) } {}

    HttpProbe::~HttpProbe() { Cancel(); }

    auto HttpProbe::Poll() -> void {
        // Bound work per editor frame even if a server continuously sends data.
        for ( int i{}; i < 16 && mImpl->mContext.poll_one() != 0; ++i ) {}
    }
    auto HttpProbe::Cancel() -> void { mImpl->Finish( Status::Cancelled, "Request cancelled." ); }
    auto HttpProbe::IsPending() const -> bool { return mImpl->IsPending(); }
    auto HttpProbe::GetStatus() const -> Status { return mImpl->mStatus; }
    auto HttpProbe::GetResponse() const -> const HttpResponse& { return mImpl->mResponse; }
    auto HttpProbe::GetError() const -> const eastl::string& { return mImpl->mError; }
    auto HttpProbe::GetUrl() const -> const eastl::string& { return mImpl->mUrl; }
    auto HttpProbe::GetReceivedBytes() const -> core::usize { return mImpl->mRaw.size(); }
    auto HttpProbe::GetElapsedMilliseconds() const -> double {
        const auto end{ IsPending() ? Impl::Clock::now() : mImpl->mFinished };
        return std::chrono::duration<double, std::milli>( end - mImpl->mStarted ).count();
    }
}
