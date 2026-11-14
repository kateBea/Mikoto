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

#ifndef MIKOTO_NETWORKING_GAME_NETWORKING_SOCKETS_HH
#define MIKOTO_NETWORKING_GAME_NETWORKING_SOCKETS_HH

/**
 * @file
 * @brief Direct access to Valve's standalone GameNetworkingSockets API.
 *
 * Applications own GameNetworkingSockets_Init/GameNetworkingSockets_Kill and
 * pump ISteamNetworkingSockets::RunCallbacks themselves. Mikoto's Asio context
 * does not manage these connections or their lifetime.
 */
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#endif // MIKOTO_NETWORKING_GAME_NETWORKING_SOCKETS_HH
