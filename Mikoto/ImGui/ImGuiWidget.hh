//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTOROOT_IMGUI_WIDGET_HH
#define MIKOTOROOT_IMGUI_WIDGET_HH

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

namespace mikoto::imgui::widget {

    auto MakeHelpPopUp( eastl::string_view description, eastl::string_view placeHolder = "(?)" ) -> void;
    auto MakeHelpPopUpDelay(eastl::string_view description, eastl::string_view placeHolder, float duration = 2.0f ) -> void;

    MKT_NODISCARD auto MakeIconTitle( eastl::string_view panelIcon, eastl::string_view panelName ) -> eastl::string;

    auto ToggleButton(
        core::cstr label,
        bool state,
        bool handOnHover,
        ImVec2 size = {0, 0},
        core::f32 alpha = 1.0f,
        core::f32 pressedAlpha = 1.0f,
        ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_None,
        ImGuiCol activeColor = ImGuiCol_ButtonActive ) -> bool;

    // =======================================================================================================================================
    // Text widgets
    // =======================================================================================================================================
    auto ToolTip( eastl::string_view description ) -> void;
    auto ToolTip( const eastl::function<void()> &func, bool enable ) -> void;


    // =======================================================================================================================================
    // Button widgets
    // =======================================================================================================================================
    auto TextArea( eastl::string &buffer ) -> bool;
    auto CheckBox( eastl::string_view label, bool &value ) -> bool;
    auto InputText( eastl::string &data, ImGuiTextFlags flags ) -> bool;
    auto ButtonTextIcon( eastl::string_view icon, ImVec2 size = { 0.0f, 0.0f } ) -> bool;
    auto PushImageButton( core::u64 textureId, ImTextureID textureHandle, ImVec2 size ) -> bool;
    auto PushImageButton( eastl::string_view ID, ImTextureID textureHandle, ImVec2 size ) -> bool;

    // =======================================================================================================================================
    // Drag widgets
    // =======================================================================================================================================
    auto Slider( eastl::string_view label, core::f32 &value, const core::float2 &bounds, eastl::string_view format = "%.2f" ) -> bool;
    auto DragFloat4( eastl::string_view label, eastl::string_view format, core::float4 &vect, core::f32 speed, core::f32 minVal, core::f32 maxVal ) -> bool;
    auto DragFloat3( eastl::string_view label, eastl::string_view format, core::float3 &vect, core::f32 speed, core::f32 minVal, core::f32 maxVal ) -> bool;

    // =======================================================================================================================================
    // Dropdown widgets
    // =======================================================================================================================================
    // Returns the index of the selected item, or -1 if no selection was made
    MKT_NODISCARD auto Combo( eastl::span<const eastl::string> choices, core::usize currentSelectionIndex ) -> core::i32;

    // =======================================================================================================================================
    // Color widgets
    // =======================================================================================================================================
    auto ColorEdit4( eastl::string_view label, core::float4 &vect ) -> bool;
    auto ColorEdit3( eastl::string_view label, core::float4 &vect ) -> bool;
}// namespace mikoto::imgui::widget

#include <ImGui/ImGuiWidget.inl>

#endif//MIKOTOROOT_IMGUI_WIDGET_HH
