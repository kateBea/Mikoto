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

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS Sources/*.cc)

target_sources(Mikoto PRIVATE ${SOURCE_FILES})

# Vendored text editor shared by the vcpkg and FetchContent builds.
# https://github.com/goossens/ImGuiColorTextEdit
# Vendored revision: 133614b0d5e1008527a26f46a93fdb1d751ca115
set(IMGUI_COLOR_TEXT_EDIT_ROOT "${MIKOTO_SOURCE_DIR}/Vendor/ImGuiColorTextEdit")
target_sources(Mikoto PRIVATE "${IMGUI_COLOR_TEXT_EDIT_ROOT}/TextEditor.cpp")
target_include_directories(Mikoto PUBLIC "${IMGUI_COLOR_TEXT_EDIT_ROOT}")

# Blueprint-example drawing helpers are needed for both vcpkg and FetchContent builds.
target_sources(Mikoto PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Vendor/imgui_node_editor/examples/blueprints-example/utilities/widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Vendor/imgui_node_editor/examples/blueprints-example/utilities/drawing.cpp")