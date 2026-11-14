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

# Shared resources and runtime dependencies for the editor and standalone player.
include_guard(GLOBAL)

function(mikoto_deploy_application target)
    # Applications own their resource target so editor-specific files can be
    # added without making this shared runtime deployment helper know about them.
    if(NOT TARGET ${target}Resources)
        add_custom_target(${target}Resources
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_SOURCE_DIR}/Mikoto/Resources" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Synchronizing ${target} resources..."
            VERBATIM)
    endif()
    add_dependencies(${target} ${target}Resources)

    # Both applications use scene scripting and deploy the header beside their resources.
    add_custom_target(${target}NativeScriptSDK
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${target}>/Resources/NativeScriptSDK/Scripting/Native"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Mikoto/Scripting/Native/NativeScriptApi.hh"
            "$<TARGET_FILE_DIR:${target}>/Resources/NativeScriptSDK/Scripting/Native/NativeScriptApi.hh"
        VERBATIM)
    add_dependencies(${target} ${target}NativeScriptSDK)
    add_dependencies(${target}NativeScriptSDK ${target}Resources)

    if(MIKOTO_USE_PRECOMPILED_SLANG)
        # Keep the SDK's compiler and matching plugins together. Applications
        # are declared before the engine, so resolve this list at generation time.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PROPERTY:slang,MIKOTO_RUNTIME_FILES>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
            VERBATIM)
    else()
        # Slang's optimizer plugin must match the statically linked compiler.
        add_dependencies(${target} mikoto-slang-glslang)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:mikoto-slang-glslang>" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()

    # Steam Audio supplies a shared runtime in both dependency modes.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:mikoto-steamaudio>" "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${target}>/Licenses/SteamAudio"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_PROPERTY:mikoto-steamaudio,MIKOTO_LICENSE_DIR>/THIRDPARTY.md"
            "${CMAKE_SOURCE_DIR}/Mikoto/Audio/SteamAudio-LICENSE.md"
            "$<TARGET_FILE_DIR:${target}>/Licenses/SteamAudio"
        VERBATIM)

    if(MIKOTO_ENABLE_PHYSX)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:${target}>/Licenses/PhysX"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PROPERTY:Mikoto,MIKOTO_PHYSX_LICENSE>"
                "$<TARGET_FILE_DIR:${target}>/Licenses/PhysX/LICENSE.md"
            VERBATIM)
    endif()

    if(APPLE)
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "@loader_path")
    elseif(UNIX)
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
    endif()
endfunction()
