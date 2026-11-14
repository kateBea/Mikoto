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
    find_package(imgui CONFIG REQUIRED)
    find_package(implot CONFIG REQUIRED)
    find_package(imguizmo CONFIG REQUIRED)

    target_link_libraries(Mikoto PUBLIC imgui::imgui implot::implot imguizmo::imguizmo)

    target_include_directories(Mikoto PUBLIC "${MIKOTO_SOURCE_DIR}/Vendor/ImOGuizmo"
        "${MIKOTO_SOURCE_DIR}/Vendor/imgui_node_editor")

    target_compile_definitions(Mikoto PUBLIC IMGUI_USE_WCHAR32 IMGUI_IMPL_VULKAN_USE_VOLK)

    target_sources(Mikoto PRIVATE "${MIKOTO_SOURCE_DIR}/Vendor/imgui_node_editor/imgui_node_editor.cpp"
        "${MIKOTO_SOURCE_DIR}/Vendor/imgui_node_editor/imgui_node_editor_api.cpp"
        "${MIKOTO_SOURCE_DIR}/Vendor/imgui_node_editor/crude_json.cpp"
        "${MIKOTO_SOURCE_DIR}/Vendor/imgui_node_editor/imgui_canvas.cpp")

    # The registry exports these static archives without their ImGui link dependency.
    # Carry it on the imported interfaces so Unix archive link order is correct.
    set_property(TARGET implot::implot APPEND PROPERTY INTERFACE_LINK_LIBRARIES imgui::imgui)
    set_property(TARGET implot::implot APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS IMGUI_USE_WCHAR32)
    set_property(TARGET imguizmo::imguizmo APPEND PROPERTY INTERFACE_LINK_LIBRARIES imgui::imgui)
else()
    # ImGui (docking branch)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b-docking
    )
    FetchContent_MakeAvailable(imgui)

    # ImPlot (v1.0)
    FetchContent_Declare(implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG v1.0
    )
    FetchContent_MakeAvailable(implot)

    # ImGuizmo
    FetchContent_Declare(ImGuizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG 1.10
    )
    FetchContent_MakeAvailable(ImGuizmo)

    # ImOGuizmo
    # Using https://github.com/fknfilewalker/imoguizmo.git
    # It is single header
    set(IMOGUIZMO_ROOT "${MIKOTO_SOURCE_DIR}/Vendor/ImOGuizmo")
    set(IMOGUIZMO_INCLUDE_DIR "${IMOGUIZMO_ROOT}")

    # imgui-node-editor
    # https://github.com/thedmd/imgui-node-editor.git
    # Slightly modified, in Mikoto we use imgui 1.90+ which has removed some functions this library relied on
    # I modified the library and added some of the missing functions. The verison used for this change was the one from master branch
    # as of commit https://github.com/thedmd/imgui-node-editor/commit/e78e447900909a051817a760efe13fe83e6e1afc
    set(IMGUI_NO_EDITOR_ROOT "${MIKOTO_SOURCE_DIR}/Vendor/imgui_node_editor")
    set(IMGUI_NO_EDITOR_INCLUDE_DIR "${IMGUI_NO_EDITOR_ROOT}")

    target_sources(Mikoto PRIVATE
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${implot_SOURCE_DIR}/implot.cpp"
        "${implot_SOURCE_DIR}/implot_items.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
        "${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp"
        "${imguizmo_SOURCE_DIR}/src/GraphEditor.cpp"
        "${imguizmo_SOURCE_DIR}/src/ImCurveEdit.cpp"
        "${imguizmo_SOURCE_DIR}/src/ImGradient.cpp"
        "${imguizmo_SOURCE_DIR}/src/ImSequencer.cpp"
        "${IMGUI_NO_EDITOR_ROOT}/imgui_node_editor.cpp"
        "${IMGUI_NO_EDITOR_ROOT}/imgui_node_editor_api.cpp"
        "${IMGUI_NO_EDITOR_ROOT}/crude_json.cpp"
        "${IMGUI_NO_EDITOR_ROOT}/imgui_canvas.cpp")
    if(MSVC)
        target_sources(Mikoto PRIVATE
            "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp")
    endif()
    target_include_directories(Mikoto PUBLIC
        "${implot_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends" "${imguizmo_SOURCE_DIR}/src"
        "${IMOGUIZMO_INCLUDE_DIR}" "${IMGUI_NO_EDITOR_INCLUDE_DIR}")

    target_compile_definitions(Mikoto PUBLIC IMGUI_USE_WCHAR32 IMGUI_IMPL_VULKAN_USE_VOLK)
endif()