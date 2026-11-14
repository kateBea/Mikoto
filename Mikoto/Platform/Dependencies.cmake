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

if(WIN32)
    target_link_libraries(Mikoto PUBLIC dwmapi ole32 shell32 user32)
endif()

if(NOT MIKOTO_MODULE_RENDERER)
    return()
endif()

if(MIKOTO_USE_VCPKG)
    find_package(SDL3 CONFIG REQUIRED)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_VULKAN ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.18
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

target_link_libraries(Mikoto PUBLIC SDL3::SDL3)

if(MIKOTO_USE_XCB_WINDOW)
    target_link_libraries(Mikoto PUBLIC xcb xcb-keysyms xcb-xfixes xcb-xkb)
    target_compile_definitions(Mikoto PUBLIC MKT_USE_XCB_WINDOW)
endif()
if(MIKOTO_USE_WIN32_WINDOW)
    target_compile_definitions(Mikoto PUBLIC MKT_USE_WIN32_WINDOW)
endif()
