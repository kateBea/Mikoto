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
    find_package(EnTT CONFIG REQUIRED)
    find_package(yaml-cpp CONFIG REQUIRED)

    target_link_libraries(Mikoto PUBLIC EnTT::EnTT
        yaml-cpp::yaml-cpp)
else()
    # EnTT
    FetchContent_Declare(entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v4.0.0
    )
    FetchContent_MakeAvailable(entt)

    # yaml-cpp
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib")
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
    set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "Disable source formatting")
    set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install targets")
    set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp as static library")
    FetchContent_Declare(yaml_cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
    )
    FetchContent_MakeAvailable(yaml_cpp)

    target_link_libraries(Mikoto PUBLIC EnTT::EnTT yaml-cpp)
endif()
