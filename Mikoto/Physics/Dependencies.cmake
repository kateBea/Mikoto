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
    find_package(Jolt CONFIG REQUIRED)

    target_link_libraries(Mikoto PUBLIC Jolt::Jolt)
else()
    # JoltPhysics
    # Mikoto and yaml-cpp use exceptions. Jolt otherwise exports
    # _HAS_EXCEPTIONS=0 to its consumers on MSVC, changing std::exception's
    # implementation across library boundaries and corrupting error handling.
    # FORCE also repairs existing build directories configured with Jolt's default.
    set(JPH_DEBUG_RENDERER ON CACHE BOOL "Debug renderer is always enabled" FORCE)
    set(CPP_RTTI_ENABLED ON CACHE BOOL "Build Jolt with RTTI" FORCE)
    set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "Enable exceptions consistently with Mikoto and yaml-cpp" FORCE)
    FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.6.0
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(JoltPhysics)

    target_link_libraries(Mikoto PUBLIC Jolt)
    if(MSVC)
        target_compile_options(Jolt PRIVATE /wd4865 /WX- /Zc:enumTypes-)
    endif()
endif()
