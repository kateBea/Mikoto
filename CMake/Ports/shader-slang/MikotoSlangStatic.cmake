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

include(CMakeFindDependencyMacro)
find_dependency(Threads)
get_filename_component(_mikoto_slang_prefix "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
foreach(_mikoto_slang_dependency IN ITEMS compiler-core libcmark-gfm core miniz lz4_static)
    set(_mikoto_slang_target "slang::mikoto-${_mikoto_slang_dependency}")
    if(NOT TARGET "${_mikoto_slang_target}")
        add_library("${_mikoto_slang_target}" STATIC IMPORTED)
        set(_mikoto_slang_archive "${CMAKE_STATIC_LIBRARY_PREFIX}mikoto-slang-${_mikoto_slang_dependency}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        set_target_properties("${_mikoto_slang_target}" PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
            IMPORTED_LOCATION_DEBUG "${_mikoto_slang_prefix}/debug/lib/${_mikoto_slang_archive}"
            IMPORTED_LOCATION_RELEASE "${_mikoto_slang_prefix}/lib/${_mikoto_slang_archive}"
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release)
    endif()
    list(APPEND _mikoto_slang_libraries "${_mikoto_slang_target}")
endforeach()
set_property(TARGET slang::slang APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "${_mikoto_slang_libraries};Threads::Threads;${CMAKE_DL_LIBS}")
if(WIN32)
    set_property(TARGET slang::slang APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        shell32 advapi32)
endif()
unset(_mikoto_slang_prefix)
unset(_mikoto_slang_dependency)
unset(_mikoto_slang_target)
unset(_mikoto_slang_archive)
unset(_mikoto_slang_libraries)
