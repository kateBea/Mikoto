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

#ifndef MIKOTOROOT_IMGUI_WIDGET_INL
#define MIKOTOROOT_IMGUI_WIDGET_INL

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

namespace mikoto::imgui::widget {

    // Invokes ToString() on the enum type to get the string representation of the enum value
    // User needs to provide a ToString() function for the enum type.
    template<typename EnumType>
    MKT_NODISCARD auto Combo( eastl::span<EnumType> choices, EnumType currentSelection ) -> EnumType;

    template<typename InputIt, typename Pred>
    auto ComboList( InputIt start, InputIt end, eastl::string& currentSelection, Pred &&isSelectedPred, eastl::string_view label ) -> void;
}// namespace mikoto::imgui::widget

#endif//MIKOTOROOT_IMGUI_WIDGET_INL
