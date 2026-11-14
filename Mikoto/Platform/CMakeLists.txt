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

# Platform implementation files. Public headers retain their existing include paths.
file(GLOB SOURCE_FILES CONFIGURE_DEPENDS Sources/*.cc)

# When the renderer is disabled, Platform compiles only those five sources for event dispatch, 
# input actions, and native OS utilities. When the renderer is enabled, the filter is skipped.
if(NOT MIKOTO_MODULE_RENDERER)
    list(FILTER SOURCE_FILES INCLUDE REGEX "/(Utility|Event|EventSystem|CoreEvents|ActionManager)\\.cc$")
endif()

target_sources(Mikoto PRIVATE ${SOURCE_FILES})

if(WIN32)
    target_link_libraries(Mikoto PRIVATE dwrite)
endif()
