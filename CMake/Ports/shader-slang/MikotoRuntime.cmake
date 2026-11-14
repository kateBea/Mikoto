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

# This wrapper is compiled from the same source revision as slang::slang.
# Upstream installs this optional module even when a static compiler does not
# depend on it. Include it in the package build so install(EXPORT) stays valid.
set_property(TARGET slang-glsl-module PROPERTY EXCLUDE_FROM_ALL FALSE)
find_package(glslang CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(SPIRV-Tools-opt CONFIG REQUIRED)
find_package(SPIRV-Tools-link CONFIG REQUIRED)
# Slang uses glslang's source-tree include spelling (SPIRV/...), whereas the
# installed package nests those headers under include/glslang.
find_path(MIKOTO_GLSLANG_SOURCE_INCLUDE_DIR SPIRV/GlslangToSpv.h
    PATH_SUFFIXES glslang REQUIRED)
add_library(mikoto-slang-glslang MODULE "${CMAKE_CURRENT_SOURCE_DIR}/source/slang-glslang/slang-glslang.cpp")
target_compile_features(mikoto-slang-glslang PRIVATE cxx_std_20)
target_include_directories(mikoto-slang-glslang PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${MIKOTO_GLSLANG_SOURCE_INCLUDE_DIR}")
target_link_libraries(mikoto-slang-glslang PRIVATE
    glslang::glslang glslang::SPIRV SPIRV-Tools-opt SPIRV-Tools-link)
set_target_properties(mikoto-slang-glslang PROPERTIES
    EXPORT_NAME slang-glslang DEBUG_POSTFIX ""
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(WIN32)
    set_target_properties(mikoto-slang-glslang PROPERTIES OUTPUT_NAME slang-glslang)
    set(mikoto_runtime_dir bin)
else()
    set_target_properties(mikoto-slang-glslang PROPERTIES OUTPUT_NAME "slang-glslang-${SLANG_VERSION_NUMERIC}")
    set(mikoto_runtime_dir lib)
endif()
install(TARGETS mikoto-slang-glslang EXPORT MikotoSlangRuntime
    LIBRARY DESTINATION ${mikoto_runtime_dir} RUNTIME DESTINATION bin)
install(EXPORT MikotoSlangRuntime FILE mikoto-slang-glslang-config.cmake
    NAMESPACE mikoto:: DESTINATION share/mikoto-slang-glslang)

# Upstream's static export hides build-local link dependencies. Package the
# required archives explicitly so installed consumers get a complete link line.
foreach(mikoto_slang_dependency IN ITEMS compiler-core libcmark-gfm core miniz lz4_static)
    install(FILES "$<TARGET_FILE:${mikoto_slang_dependency}>" DESTINATION lib
        RENAME "${CMAKE_STATIC_LIBRARY_PREFIX}mikoto-slang-${mikoto_slang_dependency}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endforeach()
