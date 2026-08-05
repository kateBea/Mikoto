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

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Platform/PlatformWin32.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Networking/NetworkService.hh>

namespace mikoto::network {

    using namespace mikoto::core;
    using namespace mikoto::threading;

    NetworkSystem::NetworkSystem( const NetworkServiceCreateInfo & ) {
        mIoContext = eastl::make_unique<asio::io_context>();
    }

    auto NetworkSystem::CreateSocket( SocketType type, const eastl::string_view hostName, const u16 port, SecurityProtocol sp ) -> SocketHandle {
        SocketHandle handle{ SocketHandle::CreateEmpty() };

        switch (type) {
            case SocketType::eTcp:
                handle = CreateSocketTcp( hostName, port, false, sp );
                break;
            case SocketType::eUdp:
                // Not supported for now
                MKT_CORE_LOGGER_WARN( "NetworkService::CreateSocket - UDP Socket not yet supported" );
                break;
            default:
                break;
        }

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateSocket - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkSystem::CreateSocketSync( SocketType type, eastl::string_view hostName, u16 port, SecurityProtocol sp ) -> SocketHandle {
        SocketHandle handle{ SocketHandle::CreateEmpty() };

        switch (type) {
            case SocketType::eTcp:
                handle = CreateSocketTcp( hostName, port, true, sp );
                break;
            case SocketType::eUdp:
                // Not supported for now
                MKT_CORE_LOGGER_WARN( "NetworkService::CreateSocketSync - UDP Socket not yet supported" );
                break;
            default:
                break;
        }

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateSocketSync - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkSystem::CreateSocketHttp( eastl::string_view hostName, bool wait ) -> SocketHandle {
        SocketHandle handle{};

        constexpr u32 httPort{ 80 };
        handle = CreateSocketTcp( hostName, httPort, wait, SecurityProtocol::eNone );

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "CreateSocketHttp::CreateSocketHttp - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkSystem::CreateSocketHttps( eastl::string_view hostName, bool wait ) -> SocketHandle {
        SocketHandle handle{};

        constexpr u32 httPort{ 443 };
        handle = CreateSocketTcp( hostName, httPort, wait, SecurityProtocol::eTLS );

        if (handle.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "NetworkService::CreateSocketHttps - Failed to create new socket" );
        } else {
            handle->Initialize();
        }

        return handle;
    }

    auto NetworkSystem::CreateSocketTcp( const eastl::string_view hostName, const u16 port, bool wait, SecurityProtocol sp ) -> SocketHandle {
        SocketHandle handle{ SocketHandle::CreateEmpty() };

        switch (sp) {
            case SecurityProtocol::eNone:
                handle = mTcpSockets.Allocate( *mIoContext, hostName, port, wait );
                break;
            case SecurityProtocol::eTLS:
#if defined( MIKOTO_OPENSSL_AVAILABLE )
                handle = mTcpSockets.Allocate( mIoContext, m_SslContext, hostName, port, wait );
#else
                MKT_CORE_LOGGER_WARN("NetworkService::CreateSocketTcp - Attempting to create TLS Socket but OpenSSL not available.");
#endif
                break;
        }

        return handle;
    }

    auto NetworkSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing NetworkService..." );

        mTcpSockets.Init( 10 );

        mIsInitialized = true;
    }

    auto NetworkSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down NetworkSystem..." );

        for (auto &socket: mTcpSockets | std::views::values) {
            // Wait for pending connections to finish
            while ( socket.As<TcpSocket>()->IsConnectionStatus( ConnectionStatus::ePending ))
                ;
        }

        // Run pending work if any
        mIoContext->run();

        mTcpSockets.Shutdown();

        mIsInitialized = false;
    }

    auto NetworkSystem::Update( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mIoContext->poll();
    }
}
