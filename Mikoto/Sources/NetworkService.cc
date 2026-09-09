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

#include <Logging/Logger.hh>
#include <Networking/NetworkService.hh>

namespace mikoto::network {

    NetworkSystem::NetworkSystem( const NetworkServiceCreateInfo& options )
        : mOptions{ options }
    {

    }

    NetworkSystem::~NetworkSystem() {
        Shutdown();
    }

    auto NetworkSystem::Initialize() -> void {
        if ( mIsInitialized ) return;
        mContext.restart();
        mWorkGuard = eastl::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>( mContext.get_executor() );
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::error_code error{};
        mTlsContext.set_default_verify_paths( error );
        if ( error ) MKT_CORE_LOGGER_WARN( "TLS system trust store is unavailable: {}", error.message() );
#endif
        mTcpSockets.Init( mOptions.mInitialSocketPoolSize );
        if ( mOptions.mUseWorkerThread ) mWorker = std::jthread( [this] { mContext.run(); } );
        mIsInitialized = true;
    }

    auto NetworkSystem::Shutdown() -> void {
        if ( !mIsInitialized ) return;
        mWorkGuard.reset();
        mContext.stop();
        if ( mWorker.joinable() ) mWorker.join();
        mTcpSockets.Shutdown();
        mIsInitialized = false;
    }

    auto NetworkSystem::Update( float ) -> void {
        if ( mIsInitialized && !mOptions.mUseWorkerThread ) mContext.poll();
    }

    auto NetworkSystem::CreateSocket( const SocketType type, const eastl::string_view host, const core::u16 port, const SecurityProtocol security ) -> SocketHandle {
        return type == SocketType::eTcp ? CreateTcpSocket( host, port, false, security ) : SocketHandle::CreateEmpty();
    }

    auto NetworkSystem::CreateSocketSync( const SocketType type, const eastl::string_view host, const core::u16 port, const SecurityProtocol security ) -> SocketHandle {
        return type == SocketType::eTcp ? CreateTcpSocket( host, port, true, security ) : SocketHandle::CreateEmpty();
    }

    auto NetworkSystem::CreateSocketHttp( const eastl::string_view host, const bool wait ) -> SocketHandle {
        return CreateTcpSocket( host, 80, wait, SecurityProtocol::eNone );
    }

    auto NetworkSystem::CreateSocketHttps( const eastl::string_view host, const bool wait ) -> SocketHandle {
        return CreateTcpSocket( host, 443, wait, SecurityProtocol::eTLS );
    }

    auto NetworkSystem::GetContext() -> asio::io_context& {
        return mContext;
    }

    auto NetworkSystem::CreateTcpSocket( const eastl::string_view host, const core::u16 port, const bool synchronous, const SecurityProtocol security ) -> SocketHandle {
        if ( !mIsInitialized || host.empty() || port == 0 ) return SocketHandle::CreateEmpty();
        if ( security == SecurityProtocol::eTLS && !HasTlsSupport() ) {
            MKT_CORE_LOGGER_WARN( "HTTPS requested for '{}' but Mikoto was built without OpenSSL.", host );
            return SocketHandle::CreateEmpty();
        }
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        auto handle{ security == SecurityProtocol::eTLS ? mTcpSockets.Allocate( mContext, mTlsContext, host, port, synchronous )
            : mTcpSockets.Allocate( mContext, host, port, synchronous ) };
#else
        auto handle{ mTcpSockets.Allocate( mContext, host, port, synchronous ) };
#endif
        SocketHandle socket{ handle };
        socket->Initialize();
        if ( synchronous && !socket->IsConnected() ) {
            return SocketHandle::CreateEmpty();
        }

        return socket;
    }
}// namespace mikoto::network
