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

get_filename_component(_ozz_prefix "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
foreach(_ozz_library IN ITEMS ozz_base ozz_animation ozz_animation_offline ozz_options ozz_animation_tools mikoto-ozz-json)
    if(NOT TARGET mikoto-ozz::${_ozz_library})
        add_library(mikoto-ozz::${_ozz_library} STATIC IMPORTED)
        set_target_properties(mikoto-ozz::${_ozz_library} PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
            IMPORTED_LOCATION_RELEASE "${_ozz_prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${_ozz_library}${CMAKE_STATIC_LIBRARY_SUFFIX}"
            IMPORTED_LOCATION_DEBUG "${_ozz_prefix}/debug/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${_ozz_library}${CMAKE_STATIC_LIBRARY_SUFFIX}"
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
            INTERFACE_INCLUDE_DIRECTORIES "${_ozz_prefix}/include")
    endif()
endforeach()
set_property(TARGET mikoto-ozz::ozz_animation PROPERTY INTERFACE_LINK_LIBRARIES mikoto-ozz::ozz_base)
set_property(TARGET mikoto-ozz::ozz_animation_offline PROPERTY INTERFACE_LINK_LIBRARIES mikoto-ozz::ozz_animation)
set_property(TARGET mikoto-ozz::ozz_options PROPERTY INTERFACE_LINK_LIBRARIES mikoto-ozz::ozz_base)
set_property(TARGET mikoto-ozz::ozz_animation_tools PROPERTY INTERFACE_LINK_LIBRARIES
    "mikoto-ozz::ozz_animation_offline;mikoto-ozz::ozz_options;mikoto-ozz::mikoto-ozz-json")
unset(_ozz_library)
unset(_ozz_prefix)
