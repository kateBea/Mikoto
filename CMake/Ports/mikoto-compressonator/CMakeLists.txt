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

cmake_minimum_required(VERSION 3.22)

project(MikotoCompressonator LANGUAGES C CXX)

set(PROJECT_FOLDER_SDK_LIBS "Dependencies/Compressonator")
set(OPTION_CMP_GTC OFF)
set(OPTION_BUILD_BROTLIG OFF)
add_subdirectory("${COMPRESSONATOR_SOURCE_DIR}/cmp_core" cmp_core)

# Upstream exposes source-tree includes; export only the public CPU API.
set_property(TARGET CMP_Core PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
target_include_directories(CMP_Core PUBLIC
    "$<BUILD_INTERFACE:${COMPRESSONATOR_SOURCE_DIR}/cmp_core/source>"
    "$<INSTALL_INTERFACE:include>")
install(TARGETS CMP_Core CMP_Core_SSE CMP_Core_AVX CMP_Core_AVX512
    EXPORT CompressonatorTargets ARCHIVE DESTINATION lib)
install(FILES "${COMPRESSONATOR_SOURCE_DIR}/cmp_core/source/cmp_core.h" DESTINATION include)
install(EXPORT CompressonatorTargets NAMESPACE Compressonator::
    FILE mikoto-compressonator-config.cmake DESTINATION lib/cmake/mikoto-compressonator)

# Upstream spells the MSVC switch /arch:AVX-512; cl expects /arch:AVX512.
if(MSVC)
    set_property(TARGET CMP_Core_AVX512 PROPERTY COMPILE_OPTIONS /arch:AVX512)
endif()
