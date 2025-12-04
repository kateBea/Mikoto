//    Copyright 2025 ケイト
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

auto main( const int, char** ) -> int {
    using namespace Mikoto;

    // Initialize Networking
    NetworkService ns{ NetworkServiceCreateInfo{} };
    ns.Init();

    // Can start python server with [ python -m http.server 8000 ]
    SocketHandle socket{ ns.CreateSocket( SocketType::SOCKET_TCP, "localhost", 8000 ) };
    if (socket->IsConnectionStatus(ConnectionStatus::CONNECTED)) {
        MKT_CORE_LOGGER_DEBUG( "Connected to address" );

        // This message should appear on the console or
        // wherever the listener is writing incoming messages
        // Will show a 400 with this message as this is not a proper HTTP request message
        socket->SendSync( "Hello from Mikoto Sockets example" );

    }

    if (socket->IsConnectionStatus(ConnectionStatus::DISCONNECTED)) {
        MKT_CORE_LOGGER_ERROR( "Failed to connect to address" );
    }

    // Free the handle
    socket.Reset();

    // Close networking system
    ns.Shutdown();

    return 0;
}