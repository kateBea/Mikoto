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

include(FetchContent)
find_package(Git REQUIRED)
find_package(Threads REQUIRED)

# Build the trace reader and ImGui view from Mikoto's pinned upstream release.
# https://github.com/wolfpld/tracy/blob/v0.14.1/profiler/CMakeLists.txt
FetchContent_Declare(mikoto_tracy_viewer
    URL https://codeload.github.com/wolfpld/tracy/zip/refs/tags/v0.14.1
    URL_HASH SHA256=908f3a2917fa86a247abfcf85dcf04bad1db6986a4d40f94b70512f3e9e98d5b
    SOURCE_SUBDIR cmake
    PATCH_COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${FETCHCONTENT_BASE_DIR}"
        "${GIT_EXECUTABLE}" apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/TracyViewer.patch")
FetchContent_MakeAvailable(mikoto_tracy_viewer)

# Apply viewer updates to existing and newly populated Tracy sources.
get_filename_component(tracy_source_parent "${mikoto_tracy_viewer_SOURCE_DIR}" DIRECTORY)
set(tracy_patch_command "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${tracy_source_parent}"
    "${GIT_EXECUTABLE}" apply --ignore-whitespace)
foreach(patch_name IN ITEMS TracyViewerLive.patch TracyViewerStop.patch TracyViewerSource.patch)
    set(patch_path "${CMAKE_CURRENT_LIST_DIR}/${patch_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch_path}")
    execute_process(COMMAND ${tracy_patch_command} --reverse --check "${patch_path}"
        WORKING_DIRECTORY "${mikoto_tracy_viewer_SOURCE_DIR}"
        RESULT_VARIABLE patch_applied OUTPUT_QUIET ERROR_QUIET)
    if(NOT patch_applied EQUAL 0)
        execute_process(COMMAND ${tracy_patch_command} "${patch_path}"
            WORKING_DIRECTORY "${mikoto_tracy_viewer_SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
endforeach()

block(PROPAGATE mikoto_tracy_capstone_SOURCE_DIR)
    set(PROJECT_VERSION "6.0.0-Alpha10")
    set(CAPSTONE_ARCHITECTURE_DEFAULT OFF)
    set(CAPSTONE_X86_SUPPORT ON)
    set(CAPSTONE_ARM_SUPPORT ON)
    set(CAPSTONE_AARCH64_SUPPORT ON)
    set(CAPSTONE_BUILD_MACOS_THIN ON)
    set(CAPSTONE_BUILD_STATIC_MSVC_RUNTIME ON)
    set(CAPSTONE_BUILD_LEGACY_TESTS OFF)
    set(CAPSTONE_BUILD_CSTOOL OFF)
    FetchContent_Declare(mikoto_tracy_capstone
        URL https://codeload.github.com/capstone-engine/capstone/zip/refs/tags/6.0.0-Alpha10
        URL_HASH SHA256=b42356d981d4c9e791c917b682689e8baeaa0e0db5385ded17254b1214828ee4
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(mikoto_tracy_capstone)
endblock()

FetchContent_Declare(mikoto_tracy_ppqsort
    URL https://codeload.github.com/GabTux/PPQSort/zip/refs/tags/v1.0.6
    URL_HASH SHA256=b828c63532acb46a78e61cc5875e959428177aa92022d9b7e5c15e61e4d2e11a
    SOURCE_SUBDIR include
    PATCH_COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${FETCHCONTENT_BASE_DIR}"
        "${GIT_EXECUTABLE}" apply -C1 --ignore-whitespace
        "${mikoto_tracy_viewer_SOURCE_DIR}/cmake/ppqsort-nodebug.patch"
        "${mikoto_tracy_viewer_SOURCE_DIR}/cmake/ppqsort-semaphore.patch")
FetchContent_MakeAvailable(mikoto_tracy_ppqsort)

# MD4C is used by Tracy's help and annotation views.
FetchContent_Declare(mikoto_tracy_md4c
    URL https://codeload.github.com/mity/md4c/zip/65c6c9d72cebd9a731aaa5597414ce04d9ea5de3
    URL_HASH SHA256=7ccfe8e2df97c0135fed033c2db3119c67048b9eb720ea4e4c48a4a878695d07
    SOURCE_SUBDIR include)
FetchContent_MakeAvailable(mikoto_tracy_md4c)
add_library(MikotoTracyMarkdown STATIC "${mikoto_tracy_md4c_SOURCE_DIR}/src/md4c.c")
target_include_directories(MikotoTracyMarkdown PUBLIC "${mikoto_tracy_md4c_SOURCE_DIR}/src")

find_package(zstd CONFIG QUIET)
if(TARGET zstd::libzstd_static AND NOT TARGET libzstd)
    add_library(libzstd ALIAS zstd::libzstd_static)
elseif(NOT TARGET libzstd)
    set(ZSTD_BUILD_SHARED OFF)
    set(ZSTD_BUILD_PROGRAMS OFF)
    set(ZSTD_BUILD_TESTS OFF)
    FetchContent_Declare(mikoto_tracy_zstd
        GIT_REPOSITORY https://github.com/facebook/zstd.git
        GIT_TAG v1.5.7
        SOURCE_SUBDIR build/cmake
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(mikoto_tracy_zstd)
    add_library(libzstd ALIAS libzstd_static)
endif()

find_package(nlohmann_json CONFIG QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(TracyCapstone INTERFACE)
target_include_directories(TracyCapstone INTERFACE "${mikoto_tracy_capstone_SOURCE_DIR}/include/capstone")
target_link_libraries(TracyCapstone INTERFACE capstone_static)

add_library(MikotoTracySort INTERFACE)
add_library(PPQSort::PPQSort ALIAS MikotoTracySort)
target_include_directories(MikotoTracySort INTERFACE "${mikoto_tracy_ppqsort_SOURCE_DIR}/include")
target_link_libraries(MikotoTracySort INTERFACE Threads::Threads)

include("${mikoto_tracy_viewer_SOURCE_DIR}/cmake/server.cmake")
# TracyClient also compiles these common sources. Keep reader symbols distinct.
target_compile_definitions(TracyServer PRIVATE tracy=mikoto_tracy_viewer ___tracy_set_thread_name=mikoto_tracy_viewer_set_thread_name)
if(WIN32)
    target_link_libraries(TracyServer PRIVATE advapi32 ws2_32)
endif()

set(tracy_profiler_dir "${mikoto_tracy_viewer_SOURCE_DIR}/profiler")
add_executable(MikotoTracyEmbed EXCLUDE_FROM_ALL
    "${tracy_profiler_dir}/helpers/embed.cpp"
    "${TRACY_COMMON_DIR}/tracy_lz4.cpp"
    "${TRACY_COMMON_DIR}/tracy_lz4hc.cpp")

# Generate the resources consumed by the original Tracy view sources.
set(tracy_data_dir "${CMAKE_CURRENT_BINARY_DIR}/tracy-viewer/data")
file(STRINGS "${tracy_profiler_dir}/CMakeLists.txt" tracy_assets
    REGEX "^Embed\\(PROFILER_FILES (Manual|Text[A-Za-z0-9]+) [^ ]+ TEXT\\)")
file(STRINGS "${tracy_profiler_dir}/CMakeLists.txt" tracy_fonts
    REGEX "^Embed\\(PROFILER_FILES Font[A-Za-z]+ src/font/")
list(APPEND tracy_assets ${tracy_fonts})
set(tracy_generated_sources)
foreach(asset IN LISTS tracy_assets)
    string(REGEX MATCH "^Embed\\(PROFILER_FILES ([A-Za-z0-9]+) ([^)]+)\\)" match "${asset}")
    set(name "${CMAKE_MATCH_1}")
    set(asset_path "${CMAKE_MATCH_2}")
    set(embed_flags)
    if(asset_path MATCHES " TEXT$")
        string(REGEX REPLACE " TEXT$" "" asset_path "${asset_path}")
        set(embed_flags -t)
    endif()
    string(REPLACE "\\ " " " asset_path "${asset_path}")
    set(input "${tracy_profiler_dir}/${asset_path}")
    set(output "${tracy_data_dir}/${name}")
    add_custom_command(OUTPUT "${output}.cpp" "${output}.hpp"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${tracy_data_dir}"
        COMMAND MikotoTracyEmbed ${embed_flags} "${name}" "${input}" "${output}"
        DEPENDS MikotoTracyEmbed "${input}"
        VERBATIM)
    list(APPEND tracy_generated_sources "${output}.cpp" "${output}.hpp")
endforeach()

file(GLOB tracy_view_sources "${tracy_profiler_dir}/src/profiler/*.cpp")
list(FILTER tracy_view_sources EXCLUDE REGEX "/(TracyLlm[^/]*|TracyFileselector|TracyWeb)\\.cpp$")
add_library(MikotoTracyViewer STATIC
    "${CMAKE_CURRENT_LIST_DIR}/TracyViewer.cc"
    "${tracy_profiler_dir}/src/ini.c"
    ${tracy_view_sources}
    ${tracy_generated_sources})
target_include_directories(MikotoTracyViewer PUBLIC "${CMAKE_CURRENT_LIST_DIR}" PRIVATE
    "${tracy_profiler_dir}/src" "${tracy_profiler_dir}/src/profiler" "${CMAKE_CURRENT_BINARY_DIR}/tracy-viewer"
    "$<TARGET_PROPERTY:Mikoto,INTERFACE_INCLUDE_DIRECTORIES>")
target_compile_definitions(MikotoTracyViewer PRIVATE
    tracy=mikoto_tracy_viewer MIKOTO_TRACY_EMBEDDED TRACY_NO_ROOT_WINDOW IMGUI_USE_WCHAR32)
target_link_libraries(MikotoTracyViewer PRIVATE TracyServer MikotoTracyMarkdown Threads::Threads nlohmann_json::nlohmann_json)
if(MIKOTO_USE_VCPKG)
    find_package(imgui CONFIG REQUIRED)
    target_link_libraries(MikotoTracyViewer PRIVATE imgui::imgui)
endif()
if(WIN32)
    target_compile_definitions(TracyServer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(MikotoTracyViewer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(MikotoTracyViewer PRIVATE shell32)
endif()
