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

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_git(OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/guillaumeblanc/ozz-animation.git
    REF 744eb9d99f606eda849acb0b1204f7a3dc20bca1)
# Build the importer library; standalone import executables are not engine dependencies.
vcpkg_replace_string("${SOURCE_PATH}/src/animation/offline/tools/CMakeLists.txt"
    "if(NOT EMSCRIPTEN)" "if(FALSE)")
file(APPEND "${SOURCE_PATH}/CMakeLists.txt" [=[
# GCC can mistake Ozz's temporary span for the owner of skeleton pose data.
# Keep this diagnostic visible without making it fatal; retain other -Werror checks.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
    target_compile_options(ozz_animation PRIVATE -Wno-error=dangling-reference)
endif()
set_target_properties(json PROPERTIES OUTPUT_NAME mikoto-ozz-json)
install(TARGETS json ARCHIVE DESTINATION lib)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/ozz" DESTINATION include)
]=])
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "dynamic" DYNAMIC_RUNTIME)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
    -Dozz_build_tools=ON -Dozz_build_gltf=OFF -Dozz_build_fbx=OFF
    -Dozz_build_samples=OFF -Dozz_build_howtos=OFF -Dozz_build_tests=OFF
    -Dozz_build_data=OFF -Dozz_build_simd_ref=OFF -Dozz_build_postfix=OFF
    -Dozz_build_msvc_rt_dll=${DYNAMIC_RUNTIME})
vcpkg_cmake_install()
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/mikoto-ozz-config.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/mikoto-ozz")
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
