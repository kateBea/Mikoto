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

#include <algorithm>
#include <cfloat>

#include <EASTL/string.h>

#include <Application/PropertyWidgets.hh>

#include <ImGui/ImGuiWidget.hh>

namespace mikoto::editor {

    namespace {

        // Keep hidden IDs usable inside existing property tables. Visible labels
        // own a left column; narrow inspectors put the label above the control.
        struct PropertyRow {
            bool mTable{};

            explicit PropertyRow( eastl::string_view label ) {
                ImGui::PushID( label.data(), label.data() + label.size() );
                ImGui::BeginGroup();
                const auto visible{ label.substr( 0, label.find( "##" ) ) };

                if ( !visible.empty() ) {
                    mTable = ImGui::GetContentRegionAvail().x >= 280.0f &&
                             ImGui::BeginTable( "##Property", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings );

                    if ( mTable ) {
                        ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthStretch, 2.0f );
                        ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, 3.0f );
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                    }

                    ImGui::AlignTextToFramePadding();
                    ImGui::TextWrapped( "%.*s", static_cast<int>( visible.size() ), visible.data() );

                    if ( mTable ) {
                        ImGui::TableNextColumn();
                    }
                }
            }

            ~PropertyRow() {
                if ( mTable ) {
                    ImGui::EndTable();
                }

                ImGui::EndGroup();
                ImGui::PopID();
            }
        };

        auto FloatControl( core::f32 *value, core::f32 minimum, core::f32 maximum, eastl::string_view format, bool clamp ) -> bool {
            const bool bounded{ minimum < maximum };
            // FLT_MAX is a validation limit, not a usable slider scale. Keep
            // those fields open to exact input beyond the practical track.
            const float low{ bounded ? minimum : -100.0f };
            const float high{ bounded ? ( maximum < FLT_MAX ? maximum : std::max( low + 1000.0f, 1000.0f ) ) : 100.0f };
            const bool changed{ imgui::widget::CircleSlider( "##Value", value, low, high,
                format.empty() ? "%.3f" : format, false ) };

            if ( changed && clamp && bounded ) {
                *value = std::clamp( *value, minimum, maximum );
            }

            return changed;
        }

    }// namespace

    auto DrawFloatProperty( eastl::string_view label, core::f32 *value, const core::float2 &bounds,
        eastl::string_view format, bool clampInput ) -> bool {
        const PropertyRow row{ label };
        return FloatControl( value, bounds.x, bounds.y, format, clampInput );
    }

    auto DrawFloatVectorProperty( eastl::string_view label, core::f32 *values, core::u32 count,
        const core::float2 &bounds, eastl::string_view format, bool clampInput ) -> bool {
        const PropertyRow row{ label };
        bool changed{};

        if ( ImGui::BeginTable( "##Axes", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            ImGui::TableSetupColumn( "Axis", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
            constexpr core::cstr axes[]{ "X", "Y", "Z", "W" };

            for ( core::u32 axis{}; axis < count; ++axis ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( axes[axis] );
                ImGui::TableNextColumn();
                ImGui::PushID( static_cast<int>( axis ) );
                changed |= FloatControl( &values[axis], bounds.x, bounds.y, format, clampInput );
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        return changed;
    }

}// namespace mikoto::editor
