#    Copyright 2026 ケイト
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(OpenSSL REQUIRED)

if(MIKOTO_USE_VCPKG)
    find_package(asio CONFIG REQUIRED)
    find_package(GameNetworkingSockets CONFIG REQUIRED)

    target_link_libraries(Mikoto PUBLIC asio::asio)
else()
    # Configuration for Networking

    # Asio
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-38-2
    )

    FetchContent_MakeAvailable(asio)

    # Keep the upstream's generic build options local to this dependency, so they
    # cannot change how other FetchContent libraries build their tools or tests.
    function(mikoto_fetch_game_networking_sockets)
        set(BUILD_STATIC_LIB ON)
        set(BUILD_SHARED_LIB OFF)
        set(BUILD_EXAMPLES OFF)
        set(BUILD_TESTS OFF)
        set(BUILD_TOOLS OFF)
        set(ENABLE_ICE ON)
        set(USE_STEAMWEBRTC OFF)
        set(USE_CRYPTO OpenSSL)
        set(Protobuf_USE_STATIC_LIBS ON)

        # GameNetworkingSockets requires an installed Protobuf library and protoc.
        # Use the same release as the vcpkg manifest without tracking a moving tag.
        FetchContent_Declare(
            gamenetworkingsockets
            GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
            GIT_TAG v1.6.1
            GIT_SUBMODULES ""
        )
        FetchContent_MakeAvailable(gamenetworkingsockets)
    endfunction()

    mikoto_fetch_game_networking_sockets()

    target_include_directories(Mikoto PUBLIC "${asio_SOURCE_DIR}/include")
    target_compile_definitions(Mikoto PUBLIC ASIO_STANDALONE)
endif()

set(MIKOTO_ALLOW_SSL ON)

target_link_libraries(Mikoto PUBLIC
    GameNetworkingSockets::GameNetworkingSockets_s OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(Mikoto PUBLIC MIKOTO_OPENSSL_AVAILABLE)
if(WIN32)
    target_link_libraries(Mikoto PUBLIC ws2_32 mswsock)
endif()
