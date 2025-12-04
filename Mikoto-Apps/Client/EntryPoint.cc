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
#include <Networking/Client.hh>
#include <Networking/NetworkService.hh>
#include <Library/String/String.hh>

auto main( const int, char** ) -> int {
    using namespace Mikoto;

    // Initialize Networking
    NetworkService ns{ NetworkServiceCreateInfo{} };
    ns.Init();

    HttpClient httpClient{ "localhost", 8000 };
    HttpResponse httpResponse{ httpClient.Get( "/hello", "text/plain" ) };

    MKT_CORE_LOGGER_INFO( "Status {}", httpResponse.Status );
    MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "{}", httpResponse.Body );

    // Close networking system
    ns.Shutdown();

    return 0;
}