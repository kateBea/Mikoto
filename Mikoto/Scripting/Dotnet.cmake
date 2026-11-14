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

# Optional CoreCLR hosting. Consume the installed .NET SDK's official native
# hosting pack; do not fetch/build the legacy standalone Mono runtime.
option(MIKOTO_ENABLE_DOTNET_SCRIPTING "Enable C# scripting through CoreCLR/hostfxr" OFF)
target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_DOTNET_SCRIPTING=$<BOOL:${MIKOTO_ENABLE_DOTNET_SCRIPTING}>)

if(NOT MIKOTO_ENABLE_DOTNET_SCRIPTING)
    return()
endif()


# Scripting with C# uses dotnet projects
find_program(MIKOTO_DOTNET_EXECUTABLE NAMES dotnet REQUIRED)

execute_process(COMMAND "${MIKOTO_DOTNET_EXECUTABLE}" --list-sdks
    OUTPUT_VARIABLE mikoto_dotnet_sdks RESULT_VARIABLE mikoto_dotnet_sdk_status)

if(NOT mikoto_dotnet_sdk_status EQUAL 0 OR NOT mikoto_dotnet_sdks MATCHES "(^|\n)10\\.")
    message(FATAL_ERROR "C# scripting requires the .NET 10 SDK (the runtime alone is insufficient). Install the SDK or disable MIKOTO_ENABLE_DOTNET_SCRIPTING.")
endif()

get_filename_component(mikoto_dotnet_real "${MIKOTO_DOTNET_EXECUTABLE}" REALPATH)
get_filename_component(mikoto_dotnet_root "${mikoto_dotnet_real}" DIRECTORY)

set(MIKOTO_DOTNET_ROOT "${mikoto_dotnet_root}" CACHE PATH ".NET SDK installation containing packs/")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(mikoto_dotnet_arch arm64)
else()
    set(mikoto_dotnet_arch x64)
endif()

if(WIN32)
    set(mikoto_dotnet_os win)
elseif(APPLE)
    set(mikoto_dotnet_os osx)
elseif(UNIX)
    set(mikoto_dotnet_os linux)
else()
    message(FATAL_ERROR "Unsupported CoreCLR host platform.")
endif()

set(MIKOTO_DOTNET_RID "${mikoto_dotnet_os}-${mikoto_dotnet_arch}" CACHE STRING ".NET native host runtime identifier")

file(GLOB mikoto_dotnet_packs LIST_DIRECTORIES TRUE
    "${MIKOTO_DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.${MIKOTO_DOTNET_RID}/10.*")
list(SORT mikoto_dotnet_packs COMPARE NATURAL ORDER DESCENDING)

if(NOT mikoto_dotnet_packs)
    message(FATAL_ERROR "Missing .NET 10 native hosting pack for ${MIKOTO_DOTNET_RID} under ${MIKOTO_DOTNET_ROOT}/packs.")
endif()

list(GET mikoto_dotnet_packs 0 mikoto_dotnet_pack)

set(mikoto_dotnet_native "${mikoto_dotnet_pack}/runtimes/${MIKOTO_DOTNET_RID}/native")

find_path(MIKOTO_DOTNET_INCLUDE_DIR nethost.h PATHS "${mikoto_dotnet_native}" NO_DEFAULT_PATH REQUIRED)

if(WIN32)
    # The SDK's static libnethost.lib uses the release CRT and iterator ABI.
    # Use the DLL import library so Debug builds retain Mikoto's own CRT settings.
    # A distinct cache variable also avoids reusing the old static-library path.
    find_file(MIKOTO_NETHOST_IMPORT_LIBRARY nethost.lib PATHS "${mikoto_dotnet_native}" NO_DEFAULT_PATH REQUIRED)
    find_file(MIKOTO_NETHOST_RUNTIME nethost.dll PATHS "${mikoto_dotnet_native}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(Mikoto PRIVATE "${MIKOTO_NETHOST_IMPORT_LIBRARY}")
else()
    find_file(MIKOTO_NETHOST_LIBRARY libnethost.a PATHS "${mikoto_dotnet_native}" NO_DEFAULT_PATH REQUIRED)
    target_compile_definitions(Mikoto PRIVATE NETHOST_USE_AS_STATIC)
    target_link_libraries(Mikoto PRIVATE "${MIKOTO_NETHOST_LIBRARY}" ${CMAKE_DL_LIBS})
endif()

target_include_directories(Mikoto PRIVATE "${MIKOTO_DOTNET_INCLUDE_DIR}")

# Build only the internal bridge/compiler. User .cs files are compiled by the
# editor's script build monitor or Play preflight, outside the native build.
set(mikoto_script_core_output "${CMAKE_CURRENT_BINARY_DIR}/script-core/$<CONFIG>")

add_custom_target(MikotoScriptCore
    COMMAND "${CMAKE_COMMAND}" -E env DOTNET_CLI_TELEMETRY_OPTOUT=1
        "${MIKOTO_DOTNET_EXECUTABLE}" build "${CMAKE_CURRENT_LIST_DIR}/Dotnet/Mikoto.ScriptCore.csproj"
        --configuration "$<IF:$<CONFIG:Debug>,Debug,Release>"
        --artifacts-path "${CMAKE_CURRENT_BINARY_DIR}/script-core-artifacts"
        --output "${mikoto_script_core_output}"
    COMMENT "Building Mikoto's internal C# bridge and compiler (.NET 10)"
    VERBATIM)

add_dependencies(Mikoto MikotoScriptCore)

foreach(app IN ITEMS MikotoEditor MikotoRuntime)
    if(NOT TARGET ${app})
        continue()
    endif()

    add_custom_target(${app}ScriptCoreStage
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${app}>/Resources/Scripting"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${mikoto_script_core_output}" "$<TARGET_FILE_DIR:${app}>/Resources/Scripting"
        DEPENDS MikotoScriptCore
        COMMENT "Staging C# assemblies for ${app}"
        VERBATIM)

    if(WIN32)
        add_custom_command(TARGET ${app}ScriptCoreStage POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MIKOTO_NETHOST_RUNTIME}" "$<TARGET_FILE_DIR:${app}>/nethost.dll"
            VERBATIM)
    endif()

    add_dependencies(${app} ${app}ScriptCoreStage)
endforeach()
