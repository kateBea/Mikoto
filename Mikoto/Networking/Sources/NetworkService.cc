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

#include <Networking/TcpSocket.hh>
#include <Networking/NetworkService.hh>

namespace mikoto::network {

    NetworkService::NetworkService( const NetworkServiceCreateInfo &options )
        : mOptions{ options } {
    }

    NetworkService::~NetworkService() {
        MKT_BEGIN_PROFILER();
        Shutdown();
    }

    auto NetworkService::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        if ( !mContext->active ) {
            mContext = std::make_shared<SocketContext>();
        }

        MKT_CORE_LOGGER_INFO( "Initializing NetworkService..." );
        mContext->context.restart();
        mWorkGuard = eastl::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>( mContext->context.get_executor() );
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::error_code error{};
        mContext->tls.set_default_verify_paths( error );

        if ( error ) {
            MKT_CORE_LOGGER_WARN( "TLS system trust store is unavailable: {}", error.message() );
        }
#endif
        if ( mOptions.mUseWorkerThread ) {
            mWorker = std::jthread( [this] {
                mContext->context.run();
            } );
        }

        mIsInitialized = true;
    }

    auto NetworkService::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down NetworkService..." );

        // Close the submission gate before scheduling cancellation on each strand.
        {
            std::lock_guard lock{ mContext->mutex };
            mContext->active = false;

            for ( const auto &entry: mContext->requests ) {
                if ( auto request{ entry.lock() } ) {
                    request->CancelRequest();
                }
            }

            mContext->requests.clear();
        }

        mWorkGuard.reset();
        mContext->context.stop();

        if ( mWorker.joinable() ) {
            mWorker.join();
        }

        {
            std::lock_guard lock{ mContext->mutex };

            for ( auto *socket: mContext->sockets ) {
                socket->Disconnect();
            }
        }

        // Drain cancelled operations to release callback-owned Refs. No new
        // connections can start on this context once active becomes false.
        mContext->context.restart();
        mContext->context.run();

        mIsInitialized = false;
    }

    auto NetworkService::PollTasks() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized || !mContext->active || Sleeping() || mOptions.mUseWorkerThread ) {
            return;
        }

        // A handler can enqueue another handler. Limit one frame's work so a
        // busy connection cannot starve input, simulation and rendering.
        for ( core::u32 handled{}; handled < 256 && mContext->context.poll_one(); ++handled ) {
        }
    }

    auto NetworkService::CreateSocket( const SocketType type, const eastl::string_view host, const core::u16 port, const SecurityProtocol security ) -> SocketHandle {
        MKT_BEGIN_PROFILER();
        return type == SocketType::eTcp ? CreateTcpSocket( host, port, false, security ) : SocketHandle::CreateEmpty();
    }

    auto NetworkService::CreateSocketSync( const SocketType type, const eastl::string_view host, const core::u16 port, const SecurityProtocol security ) -> SocketHandle {
        MKT_BEGIN_PROFILER();
        return type == SocketType::eTcp ? CreateTcpSocket( host, port, true, security ) : SocketHandle::CreateEmpty();
    }

    auto NetworkService::CreateSocketHttp( const eastl::string_view host, const bool wait ) -> SocketHandle {
        MKT_BEGIN_PROFILER();
        return CreateTcpSocket( host, 80, wait, SecurityProtocol::eNone );
    }

    auto NetworkService::CreateSocketHttps( const eastl::string_view host, const bool wait ) -> SocketHandle {
        MKT_BEGIN_PROFILER();
        return CreateTcpSocket( host, 443, wait, SecurityProtocol::eTls );
    }

    auto NetworkService::GetContext() -> asio::io_context & {
        MKT_BEGIN_PROFILER();
        return mContext->context;
    }

    auto NetworkService::CreateTcpSocket( const eastl::string_view host, const core::u16 port, const bool synchronous, const SecurityProtocol security ) -> SocketHandle {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized || host.empty() || port == 0 ) {
            return SocketHandle::CreateEmpty();
        }

        if ( security == SecurityProtocol::eTls && !HasTlsSupport() ) {
            MKT_CORE_LOGGER_WARN( "HTTPS requested for '{}' but Mikoto was built without OpenSSL.", host );
            return SocketHandle::CreateEmpty();
        }

        TcpSocketHandle handle{ TcpSocketHandle::New( mContext, security ) };
        SocketHandle socket{ handle };

        // Construction only creates the handle.
        // Choose the connection operation explicitly.
        if ( synchronous ) {
            if ( !socket->Connect( host, port ) ) {
                return SocketHandle::CreateEmpty();
            }
        }
        else {
            socket->ConnectAsync( host, port, {} );
        }

        return socket;
    }
}// namespace mikoto::network
