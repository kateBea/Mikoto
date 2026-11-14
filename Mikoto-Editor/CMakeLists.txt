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

project(MikotoEditor
        VERSION 1.0
        LANGUAGES CXX)

option(MIKOTO_ENABLE_DOTNET_SCRIPTING "Enable C# scripting through CoreCLR/hostfxr" ON)
option(MIKOTO_EDITOR_NETWORKING "Enable networking in the editor" ON)
option(MIKOTO_EDITOR_TRACY_VIEWER "Embed the Tracy profiler in the editor" ON)

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS Sources/*.cc)
if(NOT MIKOTO_EDITOR_TRACY_VIEWER)
    list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Sources/InstrumentationPanel.cc")
endif()

# Keep a console during startup debugging. This option selects both the entry
# point and linker subsystem so WinMain can be restored with one switch.
option(MIKOTO_EDITOR_USE_WINMAIN "Launch the Windows editor without a console" OFF)
if(WIN32 AND MIKOTO_EDITOR_USE_WINMAIN)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE MIKOTO_EDITOR_USE_WINMAIN=1)
else()
    add_executable(${PROJECT_NAME} ${SOURCE_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MIKOTO_EDITOR_USE_WINMAIN=0)
endif()

if(MSVC)
    # File too complex for MSVC to compile with default settings.
    set_property(SOURCE Sources/InspectorPanel.cc APPEND PROPERTY COMPILE_OPTIONS /bigobj)
endif()

# Editor branding requires only an ICO for the executable and a PNG for the UI.
set(MIKOTO_EDITOR_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/Resources/MikotoEditor.ico" CACHE FILEPATH "Editor executable icon")
set(MIKOTO_EDITOR_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/MikotoEditor.png" CACHE FILEPATH "Editor window and title-bar icon")
foreach(icon IN ITEMS MIKOTO_EDITOR_ICON_ICO MIKOTO_EDITOR_ICON_PNG)
    if(NOT EXISTS "${${icon}}")
        message(FATAL_ERROR "${icon} does not exist: ${${icon}}")
    endif()
endforeach()

if(WIN32)
    enable_language(RC)
    configure_file(Resources/Editor.rc.in "${CMAKE_CURRENT_BINARY_DIR}/Editor.rc" @ONLY)

    # Recompile the embedded icon when the ICO changes, even if the RC template does not.
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/Editor.rc" APPEND PROPERTY
        OBJECT_DEPENDS "${MIKOTO_EDITOR_ICON_ICO}")
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Editor.rc")
endif()

# The application owns its module requirements. Mikoto resolves dependencies
# and compiles the union requested by its consumers. FetchContent also limits
# downloads to that union, vcpkg installs the project's pinned manifest.
mikoto_use_modules(${PROJECT_NAME} PRIVATE
    Core
    Animation
    Assets
    Audio
    ImGui
    Material
    Physics
    Project
    Renderer
    Scene
    Scripting
    Text
)
if(MIKOTO_EDITOR_NETWORKING)
    mikoto_use_modules(${PROJECT_NAME} PRIVATE Networking)
endif()

if(MIKOTO_EDITOR_TRACY_VIEWER)
    include("${CMAKE_SOURCE_DIR}/CMake/TracyViewer.cmake")
    target_link_libraries(${PROJECT_NAME} PRIVATE MikotoTracyViewer)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MIKOTO_EDITOR_TRACY_VIEWER)
endif()

# Editor-specific includes
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(${PROJECT_NAME} PROPERTIES PROJECT_LABEL "Mikoto-Editor")

include("${CMAKE_SOURCE_DIR}/CMake/Deployment.cmake")

# Keep editor-only branding in the editor resource target while the shared
# deployment helper handles the common runtime files for both applications.
add_custom_target(${PROJECT_NAME}Resources
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_SOURCE_DIR}/Mikoto/Resources" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources/Editor"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${MIKOTO_EDITOR_ICON_PNG}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources/Editor/MikotoEditor.png"
    COMMENT "Synchronizing ${PROJECT_NAME} resources..."
    VERBATIM)
mikoto_deploy_application(${PROJECT_NAME})
