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
    find_package(sol2 CONFIG REQUIRED)

    # The pinned vcpkg package exports sol2 without a namespace.
    if(NOT TARGET sol2::sol2 AND TARGET sol2)
        add_library(sol2::sol2 ALIAS sol2)
    endif()

    find_package(Lua 5.5 REQUIRED)
    find_package(yaml-cpp CONFIG REQUIRED)

    target_link_libraries(Mikoto PUBLIC sol2::sol2 ${LUA_LIBRARIES} yaml-cpp::yaml-cpp)
    target_include_directories(Mikoto PUBLIC "${LUA_INCLUDE_DIR}")
else()
    # Build the same Lua release on every platform; avoid a bundled Windows ABI.
    FetchContent_Declare(lua
        URL https://www.lua.org/ftp/lua-5.5.1.tar.gz
        URL_HASH SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce)
    FetchContent_MakeAvailable(lua)

    file(GLOB lua_sources "${lua_SOURCE_DIR}/src/*.c")
    list(FILTER lua_sources EXCLUDE REGEX "/(lua|luac)\\.c$")
    add_library(mikoto-lua STATIC ${lua_sources})
    target_include_directories(mikoto-lua PUBLIC "${lua_SOURCE_DIR}/src")
    if(UNIX)
        target_compile_definitions(mikoto-lua PRIVATE LUA_USE_POSIX LUA_USE_DLOPEN)
        target_link_libraries(mikoto-lua PUBLIC ${CMAKE_DL_LIBS} m)
    endif()

    # Match vcpkg's sol2 compatibility fix for the Lua 5.5 API.
    find_package(Git REQUIRED)
    FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG v3.5.0
        PATCH_COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace
        "${CMAKE_CURRENT_LIST_DIR}/../../CMake/Patches/Sol2Lua55.patch"
    )

    FetchContent_MakeAvailable(sol2)

    target_link_libraries(Mikoto PUBLIC sol2::sol2 mikoto-lua yaml-cpp::yaml-cpp)
endif()
