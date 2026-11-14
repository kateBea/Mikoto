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

project(MikotoRuntime VERSION 1.0 LANGUAGES CXX)

# Default to C# support for the player; respect an explicit cached choice.
option(MIKOTO_ENABLE_DOTNET_SCRIPTING "Enable C# scripting through CoreCLR/hostfxr" ON)
file(GLOB runtime_sources CONFIGURE_DEPENDS Sources/*.cc)

add_executable(MikotoRuntime ${runtime_sources})

set_target_properties(MikotoRuntime PROPERTIES PROJECT_LABEL "Mikoto-Runtime")

target_include_directories(MikotoRuntime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

mikoto_use_modules(MikotoRuntime PRIVATE
    Core Animation Assets Audio Material Math Physics Project Renderer Scene Scripting Text)

include("${CMAKE_SOURCE_DIR}/CMake/Deployment.cmake")
mikoto_deploy_application(MikotoRuntime)
