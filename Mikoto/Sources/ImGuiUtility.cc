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

#include <ranges>

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_string.h>
#include <EASTL/functional.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <ImGui/ImGuiUtility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Memory/Allocator.hh>

namespace mikoto::gui {

    auto PushImageButton( eastl::string_view ID, ImTextureID textureHandle, ImVec2 size ) -> bool {
        return ImGui::ImageButton( ID.data(), textureHandle, size );
    }

    auto PushImageButton( u64 textureId, ImTextureID textureHandle, ImVec2 size ) -> bool {
        return ImGui::ImageButton( string::Format( "{}", textureId ).c_str(), textureHandle, size );
    }

    auto ComputeWidth() -> float {
        const ImGuiContext &globalContext{ *GImGui };
        const ImGuiWindow *currentWindow{ globalContext.CurrentWindow };
        float width{};

        if (globalContext.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) { width = globalContext.NextItemData.Width; } else { width = currentWindow->DC.ItemWidth; }

        if (width < 0.0f) {
            float regionAvailableX{ ImGui::GetContentRegionAvail().x };
            width = ImMax( 1.0f, regionAvailableX + width );
        }

        width = IM_TRUNC( width );

        return width;
    }


    auto CheckBox( eastl::string_view label, bool &value ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 0.8f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };

        ImGuiScopedColor borderColor{ ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.3f) };

        bool active{ ImGui::Checkbox( label.data(), MKT_ADDRESSOF( value ) ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto ToolTip( const eastl::string_view description ) -> void {
        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if (ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
            ImGui::TextUnformatted( description.data() );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    auto ToolTip( const eastl::function<void()> &func, const bool enable ) -> void {
        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if (enable && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );

            func();

            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    auto DragFloat4( eastl::string_view label, eastl::string_view format, glm::vec4 &vect, float speed, float minVal, float maxVal ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat4( label.data(), value_ptr( vect ), speed, minVal, maxVal, format.data() ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto DragFloat3( eastl::string_view label, eastl::string_view format, glm::vec3 &vect, float speed, float minVal, float maxVal ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat3( label.data(), value_ptr( vect ), speed, minVal, maxVal, format.data() ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto DragFloat3( eastl::string_view label, c_str format, glm::vec3 &vect, float speed, float minVal, float maxVal ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat3( label.data(), value_ptr( vect ), speed, minVal, maxVal, format ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto ColorEdit4( eastl::string_view label, glm::vec4 &vect ) -> bool {
        constexpr ImGuiColorEditFlags colorEditFlags{
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar
        };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        const bool active{ ImGui::ColorEdit4( label.data(), value_ptr( vect ), colorEditFlags ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto ColorEdit3( eastl::string_view label, glm::vec3 &vect ) -> bool {
        constexpr ImGuiColorEditFlags colorEditFlags{
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar
        };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        const bool active{ ImGui::ColorEdit3( label.data(), value_ptr( vect ), colorEditFlags ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto Slider( eastl::string_view label, float &value, const glm::vec2 &bounds, eastl::string_view format ) -> bool {
        constexpr ImGuiSliderFlags flags{ ImGuiSliderFlags_None };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        const bool active{ ImGui::SliderFloat( label.data(), std::addressof( value ), bounds.x, bounds.y, format.data(), flags ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto ButtonTextIcon( eastl::string_view icon, ImVec2 size ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 0.0f, 0.0f } };
        ImGuiScopedStyleVar framePadding{ ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } };

        const bool active{ ImGui::Button( string::Format( "{}", icon ).c_str(), size ) };

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

        return active;
    }

    auto TextArea( eastl::string &buffer ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        auto resizeCallback = [](ImGuiInputTextCallbackData* data) -> int {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                auto* str{ static_cast<eastl::string*>(data->UserData) };
                str->resize(data->BufTextLen);
                data->Buf = str->data();
            }
            return 0;
        };

        constexpr ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AllowTabInput |
            ImGuiInputTextFlags_CallbackResize };

        const ImVec2 size{ ComputeWidth(), ImGui::GetWindowSize().y * 0.3f };

        const bool active{ ImGui::InputTextMultiline(
            "##TextArea:Input",
            buffer.data(),
            buffer.capacity() + 1,
            size, flags, resizeCallback,
            MKT_ADDRESSOF( buffer )
        ) };

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_TextInput );
        }

        return active;
    }

    auto CenteredText( const char *label, const float width, float height ) -> void {
        // https://github.com/phicore/ImGuiStylingTricks/wiki/Custom-MessageBox#step-5-removed-title-bar-and-homemade-centered-text

        ImGuiContext &g{ *GImGui };
        const ImGuiStyle &style{ g.Style };

        const ImVec2 textSize{ width, height };
        const ImGuiWindow *window{ ImGui::GetCurrentWindow() };

        const ImVec2 labelSize{ ImGui::CalcTextSize( label, nullptr, true ) };

        const ImVec2 minCursorPos{ window->DC.CursorPos };
        const ImVec2 itemSize{ ImGui::CalcItemSize( textSize, labelSize.x + style.FramePadding.x * 2.0f, labelSize.y + style.FramePadding.y * 2.0f ) };

        const ImVec2 maxCursorPos{ ImVec2( minCursorPos.x + itemSize.x, minCursorPos.y + itemSize.y ) };
        const ImRect alignment{ minCursorPos, maxCursorPos };

        ImGui::ItemSize( itemSize, style.FramePadding.y );

        const ImVec2 posMin{ ImVec2( alignment.Min.x + style.FramePadding.x, alignment.Min.y + style.FramePadding.y ) };
        const ImVec2 posMax{ ImVec2( alignment.Max.x - style.FramePadding.x, alignment.Max.y - style.FramePadding.y ) };

        ImGui::RenderTextClipped( posMin, posMax, label, nullptr, &labelSize, style.ButtonTextAlign, &alignment );
    }

    auto DragDropDemo() -> void {
        if (ImGui::TreeNode( "Drag and Drop" )) {
            if (ImGui::TreeNode( "Drag and drop in standard widgets" )) {
                // ColorEdit widgets automatically act as drag source and drag target.
                // They are using standardized payload strings IMGUI_PAYLOAD_TYPE_COLOR_3F and IMGUI_PAYLOAD_TYPE_COLOR_4F
                // to allow your own widgets to use colors in their drag and drop interaction.
                // Also see 'Demo->Widgets->Color/Picker Widgets->Palette' demo.
                widget::MakeHelpPopUp( "You can drag from the color squares." );
                static float col1[3] = { 1.0f, 0.0f, 0.2f };
                static float col2[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
                ImGui::ColorEdit3( "color 1", col1 );
                ImGui::ColorEdit4( "color 2", col2 );
                ImGui::TreePop();
            }

            if (ImGui::TreeNode( "Drag and drop to copy/swap items" )) {
                enum Mode {
                    Mode_Copy,
                    Mode_Move,
                    Mode_Swap
                };
                static int mode = 0;
                if (ImGui::RadioButton( "Copy", mode == Mode_Copy )) { mode = Mode_Copy; }
                ImGui::SameLine();
                if (ImGui::RadioButton( "Move", mode == Mode_Move )) { mode = Mode_Move; }
                ImGui::SameLine();
                if (ImGui::RadioButton( "Swap", mode == Mode_Swap )) { mode = Mode_Swap; }
                static const char *names[9] = {
                    "Bobby", "Beatrice", "Betty",
                    "Brianna", "Barry", "Bernard",
                    "Bibi", "Blaine", "Bryn"
                };
                for (int n = 0; n < IM_ARRAYSIZE( names ); n++) {
                    ImGui::PushID( n );
                    if (( n % 3 ) != 0) ImGui::SameLine();
                    ImGui::Button( names[n], ImVec2( 60, 60 ) );

                    // Our buttons are both drag sources and drag targets here!
                    if (ImGui::BeginDragDropSource( ImGuiDragDropFlags_None )) {
                        // Set payload to carry the index of our item (could be anything)
                        ImGui::SetDragDropPayload( "DND_DEMO_CELL", &n, sizeof( int ) );

                        // Display preview (could be anything, e.g. when dragging an image we could decide to display
                        // the filename and a small preview of the image, etc.)
                        if (mode == Mode_Copy) { ImGui::Text( "Copy %s", names[n] ); }
                        if (mode == Mode_Move) { ImGui::Text( "Move %s", names[n] ); }
                        if (mode == Mode_Swap) { ImGui::Text( "Swap %s", names[n] ); }
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( "DND_DEMO_CELL" )) {
                            IM_ASSERT( payload->DataSize == sizeof( int ) );
                            int payload_n = *( const int * )payload->Data;
                            if (mode == Mode_Copy) { names[n] = names[payload_n]; }
                            if (mode == Mode_Move) {
                                names[n] = names[payload_n];
                                names[payload_n] = "";
                            }
                            if (mode == Mode_Swap) {
                                const char *tmp = names[n];
                                names[n] = names[payload_n];
                                names[payload_n] = tmp;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode( "Drag to reorder items (simple)" )) {
                // FIXME: there is temporary (usually single-frame) ID Conflict during reordering as a same item may be submitting twice.
                // This code was always slightly faulty but in a way which was not easily noticeable.
                // Until we fix this, enable ImGuiItemFlags_AllowDuplicateId to disable detecting the issue.
                ImGui::PushItemFlag( ImGuiItemFlags_AllowDuplicateId, true );

                // Simple reordering
                widget::MakeHelpPopUp(
                        "We don't use the drag and drop api at all here! "
                        "Instead we query when the item is held but not hovered, and order items accordingly." );
                static const char *item_names[] = { "Item One", "Item Two", "Item Three", "Item Four", "Item Five" };
                for (int n = 0; n < IM_ARRAYSIZE( item_names ); n++) {
                    const char *item = item_names[n];
                    ImGui::Selectable( item );

                    if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                        int n_next = n + ( ImGui::GetMouseDragDelta( 0 ).y < 0.f ? -1 : 1 );
                        if (n_next >= 0 && n_next < IM_ARRAYSIZE( item_names )) {
                            item_names[n] = item_names[n_next];
                            item_names[n_next] = item;
                            ImGui::ResetMouseDragDelta();
                        }
                    }
                }

                ImGui::PopItemFlag();
                ImGui::TreePop();
            }

            if (ImGui::TreeNode( "Tooltip at target location" )) {
                for (int n = 0; n < 2; n++) {
                    // Drop targets
                    ImGui::Button( n ? "drop here##1" : "drop here##0" );
                    if (ImGui::BeginDragDropTarget()) {
                        ImGuiDragDropFlags drop_target_flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoPreviewTooltip;
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( IMGUI_PAYLOAD_TYPE_COLOR_4F, drop_target_flags )) {
                            IM_UNUSED( payload );
                            ImGui::SetMouseCursor( ImGuiMouseCursor_NotAllowed );
                            ImGui::SetTooltip( "Cannot drop here!" );
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Drop source
                    static ImVec4 col4 = { 1.0f, 0.0f, 0.2f, 1.0f };
                    if (n == 0) ImGui::ColorButton( "drag me", col4 );
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }

    auto ImImageVK( const ImTextureID image, const ImVec2 dim ) -> void {
        return ImGui::Image( image, dim );
    }

    auto GetStringFromUnicode( u32 codePoint ) -> eastl::string {
        eastl::fixed_string<char, 64> utf8{};
        ImTextCharToUtf8( utf8.data(), codePoint );

        return eastl::string{ utf8.c_str() };
    }

    auto DebugShowMaterialIcons() -> void {
        ImGui::Begin( "Material Icons Debug" );

        // Scrollable area
        if (ImGui::BeginChild( "IconScrollArea", ImVec2( 0, 0 ), true )) {
            const u32 ICON_MIN{ ICON_MIN_MD };
            const u32 ICON_MAX{ ICON_MAX_16_MD };
            const u32 ICONS_PER_ROW{ 16 };

            i32 count{};
            char utf8[5]{};

            for (u32 codepoint{ ICON_MIN }; codepoint <= ICON_MAX; ++codepoint) {
                ImTextCharToUtf8( utf8, codepoint );

                // Render icon as selectable button
                ImGui::PushID( codepoint );
                if (ImGui::Button( utf8, ImVec2( 24, 24 ) )) {
                    // Optional: do something on click
                }
                if (ImGui::IsItemHovered()) { ImGui::SetTooltip( "Codepoint: U+%04X (%d)", codepoint, codepoint ); }
                ImGui::PopID();

                count++;
                if (count % ICONS_PER_ROW != 0) ImGui::SameLine();
            }

            ImGui::EndChild();
        }

        ImGui::End();
    }

    auto DrawMemoryVisualizer( const void *memory, std::size_t size, std::uintptr_t baseAddress, std::size_t bytesPerRow ) -> void {
        if (memory == nullptr || size == 0) {
            ImGui::TextUnformatted( "No memory to display." );
            return;
        }

        const auto *bytes{ static_cast<const std::uint8_t *>( memory ) };

        constexpr ImGuiTableFlags flags{
            ImGuiTableFlags_Borders
            | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_ScrollY
            | ImGuiTableFlags_SizingFixedFit
        };

        if (ImGui::BeginTable( "MemoryTable", 4, flags, ImVec2{ 0.0f, 320.0f } )) {
            ImGui::TableSetupColumn( "Address", ImGuiTableColumnFlags_WidthFixed, 120.0f );
            ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 80.0f );
            ImGui::TableSetupColumn( "Hex", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Decimal", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableHeadersRow();

            for (std::size_t row{ 0 }; row < size; row += bytesPerRow) {
                const std::uintptr_t address{ baseAddress + row };

                ImGui::TableNextRow();

                // Address
                ImGui::TableSetColumnIndex( 0 );
                ImGui::Text( "0x%08llX", static_cast<unsigned long long>( address ) );

                // Offset
                ImGui::TableSetColumnIndex( 1 );
                ImGui::Text( "+0x%06llX", static_cast<unsigned long long>( row ) );

                // Hex
                ImGui::TableSetColumnIndex( 2 );
                for (std::size_t col{ 0 }; col < bytesPerRow; ++col) {
                    const std::size_t index{ row + col };
                    if (index < size) { ImGui::Text( "%02X", bytes[index] ); } else { ImGui::TextUnformatted( "  " ); }
                    ImGui::SameLine();
                }

                // Decimal
                ImGui::TableSetColumnIndex( 3 );
                for (std::size_t col{ 0 }; col < bytesPerRow; ++col) {
                    const std::size_t index{ row + col };
                    if (index < size) { ImGui::Text( "%3u", static_cast<unsigned int>( bytes[index] ) ); } else { ImGui::TextUnformatted( "   " ); }
                    ImGui::SameLine();
                }
            }

            ImGui::EndTable();
        }
    }

    auto SetCursorHandOnLastItemHovered() -> void {
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    auto Combo(eastl::string* choices, size_t count, const eastl::string& currentSelection) -> i32 {
        i32 selectionIndex{ -1 };

        const eastl::string labelName{
            string::Format( "##{}{}", __PRETTY_FUNCTION__, currentSelection )
        };

        if ( ImGui::BeginCombo( labelName.data(), currentSelection.c_str() ) ) {
            for ( size_t index{}; index < count; ++index ) {
                const eastl::string &selectionStr{ choices[index] };

                const bool isSelected{ selectionStr == currentSelection };

                if ( ImGui::Selectable(
                             fmt::format( " {}", selectionStr ).c_str(),
                             isSelected ) ) {
                    selectionIndex = as<i32>( index );
                }

                if ( ImGui::IsItemHovered() )
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if ( ImGui::IsItemHovered() )
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

        return selectionIndex;
    }

    auto InputText(eastl::string_view viewData, bool readOnly) -> bool {
        ImGuiTextFlags flags{ ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll };

        if (readOnly) {
            flags |= ImGuiInputTextFlags_ReadOnly;
        }

        constexpr u32 kLength{ 1024 };
        eastl::fixed_string<char, kLength> name{};

        std::ranges::copy( viewData, name.data() );

        return ImGui::InputText( "##DrawNameTextInputTag", name.data(), name.max_size(), flags );
    }
}