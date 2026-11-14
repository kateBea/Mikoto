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
    find_package(fmt CONFIG REQUIRED)
    find_package(argparse CONFIG REQUIRED)
    find_package(cpptrace CONFIG REQUIRED)
    find_package(unordered_dense CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    # The editor uses TOML_EXCEPTIONS=0; the packaged binary uses the other ABI.
    # Consume the installed headers in header-only mode, like FetchContent does.

    find_path(MIKOTO_TOML_INCLUDE_DIR toml++/toml.hpp REQUIRED)

    target_include_directories(Mikoto SYSTEM PUBLIC "${MIKOTO_TOML_INCLUDE_DIR}")

    find_package(EASTL CONFIG REQUIRED)
    find_package(magic_enum CONFIG REQUIRED)

    if(MIKOTO_ENABLE_TRACY_PROFILING)
        find_package(Tracy CONFIG REQUIRED)
    endif()
else()
    # fmt
    FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 12.2.0
    )
    FetchContent_MakeAvailable(fmt)

    # argparse
    # Header-only command-line parser used during application bootstrap. Keep its
    # examples and tests out of Mikoto's build; only the interface target is needed.
    set(ARGPARSE_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ARGPARSE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG v3.2
    )
    FetchContent_MakeAvailable(argparse)

    # cpptrace
    FetchContent_Declare(cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v1.0.4
    )
    FetchContent_MakeAvailable(cpptrace)

    # unordered_dense (header-only)
    FetchContent_Declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v5.3.0
    )
    FetchContent_MakeAvailable(unordered_dense)

    # nlohmann_json
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)

    # tomlplusplus
    fetchcontent_declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(tomlplusplus)

    # EASTL
    set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        EASTL
        GIT_REPOSITORY https://github.com/electronicarts/EASTL.git
        GIT_TAG        3.27.01
    )

    FetchContent_MakeAvailable(EASTL)

    # magic_enum
    FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG        v0.9.8
    )

    FetchContent_MakeAvailable(magic_enum)

    if(MIKOTO_ENABLE_TRACY_PROFILING)
        # Tracy
        # Keep client and consumers on the same configuration. On-demand capture drops
        # disconnected zone events instead of retaining a growing pre-connection trace.
        set(TRACY_ENABLE ${MIKOTO_ENABLE_TRACY_PROFILING} CACHE BOOL "Enable Tracy client" FORCE)
        set(TRACY_ON_DEMAND ON CACHE BOOL "Capture only while a Tracy viewer is connected" FORCE)
        set(TRACY_CALLSTACK OFF CACHE BOOL "Use Mikoto's explicit per-zone callstack depth" FORCE)
        set(TRACY_NO_EXIT OFF CACHE BOOL "Allow shutdown without a connected Tracy viewer" FORCE)
        FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.14.1
        )

        FetchContent_MakeAvailable(tracy)
    endif()

    target_link_libraries(Mikoto PUBLIC EABase)
    target_include_directories(Mikoto PUBLIC
        "${tomlplusplus_SOURCE_DIR}/include")
endif()

target_link_libraries(Mikoto PUBLIC
    fmt::fmt argparse::argparse cpptrace::cpptrace unordered_dense::unordered_dense
    nlohmann_json::nlohmann_json EASTL magic_enum::magic_enum)
target_compile_definitions(Mikoto PUBLIC TOML_HEADER_ONLY=1 TOML_EXCEPTIONS=0)

if(WIN32)
    target_compile_definitions(Mikoto PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(Mikoto PUBLIC pdh psapi)
endif()
