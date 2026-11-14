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

# Audio module dependency: download the pinned SDK unless a local path is provided.
include_guard(GLOBAL)
include(FetchContent)

set(MIKOTO_STEAMAUDIO_ROOT "" CACHE PATH "Steam Audio 4.8.1 SDK root (contains include/ and lib/); empty downloads the official SDK")
if(NOT MIKOTO_STEAMAUDIO_ROOT)
    FetchContent_Declare(mikoto_steamaudio
        URL https://github.com/ValveSoftware/steam-audio/releases/download/v4.8.1/steamaudio_4.8.1.zip
        URL_HASH SHA256=4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449)
    FetchContent_MakeAvailable(mikoto_steamaudio)
    set(MIKOTO_STEAMAUDIO_ROOT "${mikoto_steamaudio_SOURCE_DIR}")
endif()

if(NOT EXISTS "${MIKOTO_STEAMAUDIO_ROOT}/include/phonon.h")
    message(FATAL_ERROR "MIKOTO_STEAMAUDIO_ROOT must point to the extracted steamaudio SDK directory")
endif()

add_library(mikoto-steamaudio SHARED IMPORTED GLOBAL)
set_target_properties(mikoto-steamaudio PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${MIKOTO_STEAMAUDIO_ROOT}/include"
    MIKOTO_LICENSE_DIR "${MIKOTO_STEAMAUDIO_ROOT}")

# HRTF processing uses phonon alone. GPU acoustics plugins are deliberately not
# loaded or deployed until a simulation backend actually uses them.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]|aarch64")
    set_target_properties(mikoto-steamaudio PROPERTIES
        IMPORTED_IMPLIB "${MIKOTO_STEAMAUDIO_ROOT}/lib/windows-x64/phonon.lib"
        IMPORTED_LOCATION "${MIKOTO_STEAMAUDIO_ROOT}/lib/windows-x64/phonon.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set_target_properties(mikoto-steamaudio PROPERTIES
        IMPORTED_LOCATION "${MIKOTO_STEAMAUDIO_ROOT}/lib/linux-x64/libphonon.so")
elseif(APPLE)
    set_target_properties(mikoto-steamaudio PROPERTIES
        IMPORTED_LOCATION "${MIKOTO_STEAMAUDIO_ROOT}/lib/osx/libphonon.dylib")
else()
    message(FATAL_ERROR "Steam Audio SDK integration supports Windows x64, Linux x64 and macOS desktop targets")
endif()
