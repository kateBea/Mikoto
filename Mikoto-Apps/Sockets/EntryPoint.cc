//
// Created by kate on 11/22/25.
//

#include <Platform/Window.hh>
#include <Networking/NetworkService.hh>

auto main( const int, char** ) -> int {
    using namespace Mikoto;

    // Initialize Networking
    NetworkService ns{ NetworkServiceCreateInfo{} };
    ns.Init();

    // Can start python server with [ python -m http.server 8000 ]
    SocketHandle socket{ ns.CreateSocket( SocketType::SOCKET_TCP, "localhost", 8000 ) };
    if (socket->IsConnected()) {
        MKT_CORE_LOGGER_DEBUG( "Connected to address" );

        // This message should appear on the console or
        // wherever the listener is writing incoming messages
        // Will show a 400 with this message as this is not a proper HTTP request message
        socket->SendSync( "Hello from Mikoto Sockets example" );

    } else {
        MKT_CORE_LOGGER_ERROR( "Failed to connect to address" );
    }

    // Free the handle
    socket.Reset();

    // Close networking system
    ns.Shutdown();

    return 0;
}