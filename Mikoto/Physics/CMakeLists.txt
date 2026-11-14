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

# Physics implementation files. Public headers retain their existing include paths.
file(GLOB SOURCE_FILES CONFIGURE_DEPENDS Sources/*.cc)
if(NOT MIKOTO_ENABLE_PHYSX)
    list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Sources/PhysXBackend.cc")
endif()

target_sources(Mikoto PRIVATE ${SOURCE_FILES})

if(MSVC)
    set_property(SOURCE Sources/PhysicsWorld.cc TARGET_DIRECTORY Mikoto
        APPEND PROPERTY COMPILE_OPTIONS /bigobj)
endif()
