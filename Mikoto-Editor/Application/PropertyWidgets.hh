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

#ifndef MIKOTO_EDITOR_APPLICATION_PROPERTY_WIDGETS_HH
#define MIKOTO_EDITOR_APPLICATION_PROPERTY_WIDGETS_HH

#include <Core/Types.hh>

namespace mikoto::editor {

    /** Label, round slider and exact numeric input. Hidden labels reuse an enclosing row.
     * Equal bounds leave exact input unrestricted; FLT_MAX uses a practical slider track.
     * clampInput applies the supplied limits to manual edits when the caller requires it.
     */
    auto DrawFloatProperty( eastl::string_view label, core::f32 *value, const core::float2 &bounds = { 0, 0 },
        eastl::string_view format = "%.3f", bool clampInput = false ) -> bool;

    /** Per-axis property rows keep vectors editable without squeezing their numeric fields. */
    auto DrawFloatVectorProperty( eastl::string_view label, core::f32 *values, core::u32 count,
        const core::float2 &bounds = { 0, 0 }, eastl::string_view format = "%.3f", bool clampInput = false ) -> bool;

}// namespace mikoto::editor

#endif// MIKOTO_EDITOR_APPLICATION_PROPERTY_WIDGETS_HH
