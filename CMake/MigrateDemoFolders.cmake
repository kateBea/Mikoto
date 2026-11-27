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

# Migrate known bundled demos without overwriting local projects or caches.
if(NOT IS_DIRECTORY "${MIKOTO_DEMO_SOURCE}" OR NOT IS_DIRECTORY "${MIKOTO_DEMO_DESTINATION}")
    return()
endif()
file(REAL_PATH "${MIKOTO_DEMO_SOURCE}" _demo_source)
file(REAL_PATH "${MIKOTO_DEMO_DESTINATION}" _demo_destination)
set(_legacy_demos
    "Animation Playground|Animation_Playground.mktproj"
    "Animation_Playground|Animation_Playground.mktproj"
    "Basic Video|Video_Gallery_PathTraced.mktproj"
    "Car Demo|Car_Demo.mktproj"
    "Car_Demo|Car_Demo.mktproj"
    "Cloth and Soft Bodies Playground|Cloth_Playground.mktproj"
    "Cloth_Playground|Cloth_Playground.mktproj"
    "Clouds|Clouds.mktproj"
    "Clouds and Fog|Volumes.mktproj"
    "Clustered_Lighting|Clustered_Lighting.mktproj"
    "Dragon Transmission|Dragon_Transmission.mktproj"
    "Dragon_Transmission|Dragon_Transmission.mktproj"
    "Forest Island|Forest_Island.mktproj"
    "Forest_Island|Forest_Island.mktproj"
    "Instancing|Instancing.mktproj"
    "Instancing 67x67x67|Instancing.mktproj"
    "Mesh Volumes|Mesh_Volumes.mktproj"
    "Mesh_Volumes|Mesh_Volumes.mktproj"
    "Mikoto Sandbox|Sandbox.mktproj"
    "Optical Materials|Optical_Materials.mktproj"
    "Optical_Materials|Optical_Materials.mktproj"
    "Particle Museum|Particle_Museum.mktproj"
    "Particle_Museum|Particle_Museum.mktproj"
    "Physics Boxes|Physics_Boxes.mktproj"
    "Physics Spheres|Physics_Spheres.mktproj"
    "Physics_Boxes|Physics_Boxes.mktproj"
    "Physics_Spheres|Physics_Spheres.mktproj"
    "Sandbox|Sandbox.mktproj"
    "Skeletal Animation|Skeletal_Animation.mktproj"
    "Skeletal_Animation|Skeletal_Animation.mktproj"
    "Skin Subsurface Scattering|Skin_Subsurface.mktproj"
    "Skin_Subsurface|Skin_Subsurface.mktproj"
    "Sphere Materials|Sphere_Materials.mktproj"
    "Sphere_Materials|Sphere_Materials.mktproj"
    "Sponza - 15,000 Animated Lights|Clustered_Lighting.mktproj"
    "Tanegashima Highlands|Terrain_Coast.mktproj"
    "Terrain_Coast|Terrain_Coast.mktproj"
    "Video Neighborhood|Video_Gallery.mktproj"
    "Video_Gallery|Video_Gallery.mktproj"
    "Video_Gallery_PathTraced|Video_Gallery_PathTraced.mktproj"
    "Volumes|Volumes.mktproj"
    "Volumetric Clouds|Clouds.mktproj"
)
foreach(_entry IN LISTS _legacy_demos)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _old)
    list(GET _parts 1 _file)
    set(_previous "${_demo_destination}/${_old}")
    if(IS_SYMLINK "${_previous}" OR NOT EXISTS "${_previous}/${_file}")
        continue()
    endif()
    file(GLOB _projects LIST_DIRECTORIES false "${_demo_source}/*/${_file}")
    list(LENGTH _projects _count)
    if(NOT _count EQUAL 1)
        continue()
    endif()
    list(GET _projects 0 _project)
    get_filename_component(_source_folder "${_project}" DIRECTORY)
    get_filename_component(_name "${_source_folder}" NAME)
    set(_current "${_demo_destination}/${_name}")
    if(_previous STREQUAL _current)
        continue()
    endif()
    file(REAL_PATH "${_previous}" _resolved_previous)
    get_filename_component(_previous_parent "${_resolved_previous}" DIRECTORY)
    get_filename_component(_current_parent "${_current}" DIRECTORY)
    if(NOT _previous_parent STREQUAL _demo_destination OR NOT _current_parent STREQUAL _demo_destination)
        message(FATAL_ERROR "Demo folder migration escaped its resource directory.")
    endif()
    if(EXISTS "${_current}")
        message(STATUS "Demo migration kept existing folders: ${_old} and ${_name}")
        continue()
    endif()
    file(RENAME "${_previous}" "${_current}")
    message(STATUS "Renamed bundled demo: ${_old} -> ${_name}")
endforeach()
