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
    find_package(mikoto-ozz CONFIG REQUIRED)
    target_link_libraries(Mikoto PUBLIC mikoto-ozz::ozz_animation
        mikoto-ozz::ozz_animation_tools
        mikoto-ozz::ozz_animation_offline)
else()
    # Ozz Animation
    set(ozz_build_tools ON CACHE BOOL "" FORCE)
    set(ozz_build_gltf ON CACHE BOOL "" FORCE)
    set(ozz_build_fbx ON CACHE BOOL "" FORCE)
    set(ozz_build_samples OFF CACHE BOOL "" FORCE)
    set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
    set(ozz_build_tests OFF CACHE BOOL "" FORCE)
    set(ozz_build_data OFF CACHE BOOL "" FORCE)
    set(ozz_build_simd_ref OFF CACHE BOOL "" FORCE)
    set(ozz_build_msvc_rt_dll OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        ozz-animation
        GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
        GIT_TAG 0.17.0
    )

    FetchContent_MakeAvailable(ozz-animation)

    # GCC can mistake Ozz's temporary span for the owner of skeleton pose data.
    # Keep this diagnostic visible without making it fatal; retain other -Werror checks.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
        target_compile_options(ozz_animation PRIVATE -Wno-error=dangling-reference)
    endif()

    target_link_libraries(Mikoto PUBLIC ozz_animation ozz_animation_tools ozz_animation_offline)
endif()
