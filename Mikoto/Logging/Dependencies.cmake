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
    find_package(spdlog CONFIG REQUIRED)
else()
    # spdlog
    set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use the shared fmt dependency" FORCE)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

target_link_libraries(Mikoto PUBLIC spdlog::spdlog)
target_compile_definitions(Mikoto PUBLIC SPDLOG_FMT_EXTERNAL)
