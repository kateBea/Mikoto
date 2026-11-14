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
    find_package(Taskflow CONFIG REQUIRED)
    target_link_libraries(Mikoto PUBLIC Taskflow::Taskflow)
else()
    # Taskflow
    set(TF_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(TF_BUILD_PROFILER OFF CACHE BOOL "" FORCE)
    set(TF_BUILD_CUDA OFF CACHE BOOL "" FORCE)
    set(TF_BUILD_SYCL OFF CACHE BOOL "" FORCE)
    set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        taskflow
        GIT_REPOSITORY https://github.com/taskflow/taskflow.git
        GIT_TAG v4.1.0
    )

    FetchContent_MakeAvailable(taskflow)

    target_include_directories(Mikoto PUBLIC "${taskflow_SOURCE_DIR}")
endif()

find_package(Threads REQUIRED)
target_link_libraries(Mikoto PUBLIC Threads::Threads)
