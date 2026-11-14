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

if(MIKOTO_USE_VCPKG)
    find_path(MIKOTO_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)

    target_include_directories(Mikoto PUBLIC "${MIKOTO_MINIAUDIO_INCLUDE_DIR}")
    target_sources(Mikoto PRIVATE "${MIKOTO_SOURCE_DIR}/Audio/Sources/Miniaudio.c")
else()
    # Miniaudio
    FetchContent_Declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.25
    )
    FetchContent_MakeAvailable(miniaudio)

    target_sources(Mikoto PRIVATE "${miniaudio_SOURCE_DIR}/miniaudio.c")
    target_include_directories(Mikoto PUBLIC "${miniaudio_SOURCE_DIR}")
endif()

if(UNIX)
    target_link_libraries(Mikoto PUBLIC ${CMAKE_DL_LIBS} m)
endif()
