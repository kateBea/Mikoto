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

include_guard(GLOBAL)

# Applications request feature modules; the engine computes their transitive
# dependencies before fetching libraries or adding implementation sources.
set(MIKOTO_FOUNDATION_MODULES Core Math Memory Logging Filesystem Threading Platform)
set(MIKOTO_FEATURE_MODULES Animation Assets Audio ImGui Material Networking Physics Project Renderer Scene Scripting Text Video)
set(MIKOTO_MODULES "" CACHE STRING "Additional Mikoto feature modules (semicolon-separated)")

set(MIKOTO_ANIMATION_REQUIRES Assets)
set(MIKOTO_ASSETS_REQUIRES Animation Audio Material Renderer Text)
set(MIKOTO_IMGUI_REQUIRES Animation Assets Renderer)
set(MIKOTO_MATERIAL_REQUIRES Renderer Scene)
set(MIKOTO_PHYSICS_REQUIRES Renderer Scene)
set(MIKOTO_PROJECT_REQUIRES Scene)
set(MIKOTO_RENDERER_REQUIRES Animation Assets ImGui Physics Scene Text)
set(MIKOTO_SCENE_REQUIRES Animation Assets Audio Material Physics Renderer Scripting Text Video)
set(MIKOTO_SCRIPTING_REQUIRES Scene)
set(MIKOTO_TEXT_REQUIRES Renderer)
set(MIKOTO_VIDEO_REQUIRES Assets Audio Renderer Threading)

# Declare requirements in the consuming application's CMakeLists.txt. The root
# includes this file and configures applications before add_subdirectory(Mikoto).
function(mikoto_use_modules target visibility)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "mikoto_use_modules: target '${target}' does not exist")
    endif()

    if(NOT visibility MATCHES "^(PRIVATE|PUBLIC|INTERFACE)$")
        message(FATAL_ERROR "mikoto_use_modules requires PRIVATE, PUBLIC or INTERFACE")
    endif()

    foreach(module IN LISTS ARGN)
        if(NOT module IN_LIST MIKOTO_FOUNDATION_MODULES AND NOT module IN_LIST MIKOTO_FEATURE_MODULES)
            message(FATAL_ERROR "Unknown Mikoto module '${module}' requested by ${target}")
        endif()

        if(TARGET Mikoto)
            get_property(enabled GLOBAL PROPERTY MIKOTO_ENABLED_MODULES)

            if(NOT module IN_LIST enabled)
                message(FATAL_ERROR "Request ${module} before add_subdirectory(Mikoto), or include it in MIKOTO_MODULES beforehand.")
            endif()
        endif()
    endforeach()

    set_property(GLOBAL APPEND PROPERTY MIKOTO_REQUESTED_MODULES ${ARGN})
    # CMake resolves the engine target and its usage requirements at generation.
    target_link_libraries("${target}" ${visibility} Mikoto)
endfunction()

function(mikoto_resolve_modules)
    get_property(requested GLOBAL PROPERTY MIKOTO_REQUESTED_MODULES)
    set(pending ${MIKOTO_FOUNDATION_MODULES} ${MIKOTO_MODULES} ${requested})
    set(enabled)

    while(pending)
        list(POP_FRONT pending module)
        if(NOT module IN_LIST MIKOTO_FOUNDATION_MODULES AND NOT module IN_LIST MIKOTO_FEATURE_MODULES)
            message(FATAL_ERROR "Unknown Mikoto module '${module}'. Available modules: ${MIKOTO_FOUNDATION_MODULES};${MIKOTO_FEATURE_MODULES}")
        endif()

        if(module IN_LIST enabled)
            continue()
        endif()

        list(APPEND enabled "${module}")
        string(TOUPPER "${module}" module_upper)
        list(APPEND pending ${MIKOTO_${module_upper}_REQUIRES})
    endwhile()

    set_property(GLOBAL PROPERTY MIKOTO_ENABLED_MODULES "${enabled}")

    foreach(module IN LISTS MIKOTO_FOUNDATION_MODULES MIKOTO_FEATURE_MODULES)
        string(TOUPPER "${module}" module_upper)
        if(module IN_LIST enabled)
            set(MIKOTO_MODULE_${module_upper} ON PARENT_SCOPE)
        else()
            set(MIKOTO_MODULE_${module_upper} OFF PARENT_SCOPE)
        endif()
    endforeach()
endfunction()
