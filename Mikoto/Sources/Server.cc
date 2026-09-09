//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#include <cstdlib>
#include <iterator>
#include <string>

#include <EASTL/utility.h>

#include <Networking/Server.hh>

#include <Logging/Logger.hh>

namespace mikoto::network {

    class HttpServer::Session final : public std::enable_shared_from_this<HttpServer::Session> {
    public:
        Session( HttpServer& server, asio::io_context& context ) : mServer{ server }, mSocket{ context } {}

        auto Start() -> void {
            asio::async_read_until( mSocket, mInput, "\r\n\r\n", [self = shared_from_this()]( const asio::error_code& error, const core::usize ) {
                if ( error ) return;
                self->ReadBodyOrDispatch();
            } );
        }

    private:
        auto ReadBodyOrDispatch() -> void {
            const std::string raw{ asio::buffers_begin( mInput.data() ), asio::buffers_end( mInput.data() ) };
            const std::size_t headerEnd{ raw.find( "\r\n\r\n" ) };
            if ( headerEnd == std::string::npos ) { WriteError( "400", "Bad Request" ); return; }
            HttpRequest request{};
            if ( !ParseHttpRequest( eastl::string_view{ raw.data(), raw.size() }, request ) ) { WriteError( "400", "Bad Request" ); return; }
            const auto contentLength{ GetHttpHeader( request.mHeaders, "content-length" ) };
            if ( !contentLength ) { Dispatch(); return; }
            char* end{};
            const unsigned long expected{ std::strtoul( contentLength->c_str(), &end, 10 ) };
            if ( end == contentLength->c_str() || *end != '\0' ) { WriteError( "400", "Bad Request" ); return; }
            const core::usize received{ raw.size() - ( headerEnd + 4 ) };
            if ( received >= expected ) { Dispatch(); return; }
            asio::async_read( mSocket, mInput, asio::transfer_exactly( expected - received ), [self = shared_from_this()]( const asio::error_code& error, const core::usize ) {
                if ( error ) return;
                self->Dispatch();
            } );
        }

        auto Dispatch() -> void {
            const std::string raw{ asio::buffers_begin( mInput.data() ), asio::buffers_end( mInput.data() ) };
            HttpRequest request{};
            if ( !ParseHttpRequest( eastl::string_view{ raw.data(), raw.size() }, request ) ) { WriteError( "400", "Bad Request" ); return; }
            mOutput = SerializeHttpResponse( mServer.Dispatch( request ) );
            asio::async_write( mSocket, asio::buffer( mOutput.data(), mOutput.size() ), [self = shared_from_this()]( const asio::error_code&, const core::usize ) {
                asio::error_code ignored{};
                self->mSocket.shutdown( asio::ip::tcp::socket::shutdown_both, ignored );
                self->mSocket.close( ignored );
            } );
        }

        auto WriteError( const eastl::string_view status, const eastl::string_view reason ) -> void {
            mOutput = SerializeHttpResponse( HttpResponse{ .mStatus = eastl::string{ status }, .mReason = eastl::string{ reason } } );
            asio::async_write( mSocket, asio::buffer( mOutput.data(), mOutput.size() ), [self = shared_from_this()]( const asio::error_code&, const core::usize ) {
                asio::error_code ignored{}; self->mSocket.close( ignored );
            } );
        }

    public:
        asio::ip::tcp::socket mSocket;

    private:
        HttpServer& mServer;
        asio::streambuf mInput{};
        eastl::string mOutput{};
    };

    HttpServer::HttpServer( NetworkSystem& network ) : mNetwork{ network } {}
    HttpServer::~HttpServer() { Stop(); }

    auto HttpServer::Get( const std::string_view path, Handler handler ) -> void {
        mGetRoutes[eastl::string{ path.data(), path.size() }] = eastl::move( handler );
    }

    auto HttpServer::Post( const std::string_view path, Handler handler ) -> void {
        mPostRoutes[eastl::string{ path.data(), path.size() }] = eastl::move( handler );
    }

    auto HttpServer::Listen( const std::string_view address, const core::u16 port ) -> bool {
        if ( !mNetwork.IsInitialized() || port == 0 || IsListening() ) return false;
        asio::error_code error{};
        const auto endpoint{ asio::ip::tcp::endpoint{ asio::ip::make_address( std::string{ address }, error ), port } };
        if ( error ) return false;
        mAcceptor = eastl::make_unique<asio::ip::tcp::acceptor>( mNetwork.GetContext() );
        mAcceptor->open( endpoint.protocol(), error );
        if ( !error ) mAcceptor->set_option( asio::socket_base::reuse_address( true ), error );
        if ( !error ) mAcceptor->bind( endpoint, error );
        if ( !error ) mAcceptor->listen( asio::socket_base::max_listen_connections, error );
        if ( error ) { mAcceptor.reset(); return false; }
        mStopping = false;
        AcceptNext();
        return true;
    }

    auto HttpServer::Stop() -> void {
        mStopping = true;
        if ( mAcceptor ) { asio::error_code ignored{}; mAcceptor->close( ignored ); mAcceptor.reset(); }
    }

    auto HttpServer::IsListening() const -> bool {
        return mAcceptor && mAcceptor->is_open() && !mStopping;
    }

    auto HttpServer::AcceptNext() -> void {
        if ( !IsListening() ) return;
        auto session{ std::make_shared<Session>( *this, mNetwork.GetContext() ) };
        mAcceptor->async_accept( session->mSocket, [this, session]( const asio::error_code& error ) {
            if ( !error ) session->Start();
            if ( !mStopping ) AcceptNext();
        } );
    }

    auto HttpServer::Dispatch( const HttpRequest& request ) const -> HttpResponse {
        const auto& routes{ request.mMethod == "GET" ? mGetRoutes : mPostRoutes };
        const auto route{ routes.find( request.mPath ) };
        if ( route == routes.end() ) return HttpResponse{ .mBody = "Route not found", .mStatus = "404", .mReason = "Not Found" };
        HttpResponse response{};
        route->second( request, response );
        return response;
    }
}
