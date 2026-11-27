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
#include <cstdarg>

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Memory/Allocator.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

namespace mikoto::imgui {

    using namespace mikoto::core;

    static auto DrawRoundedCheckMark( ImDrawList& draw, ImVec2 origin, ImU32 color, float size ) -> void {
        // Use ImGui's check proportions with a lighter stroke inset from the box edges.
        // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/imgui_draw.cpp#L5588
        const float radius{ ImMax( size / 6.0f, 1.0f ) * 0.5f };
        const float span{ size - radius * 2.0f };
        const float third{ span / 3.0f };
        const ImVec2 base{ origin.x + radius, origin.y + radius };
        const ImVec2 points[]{
            { base.x, base.y + span - third * 1.5f },
            { base.x + third, base.y + span - third * 0.5f },
            { base.x + span, base.y + span - third * 2.5f }
        };

        // Fill one outline so rounded ends and the bend have no overlapping AA fringes.
        const float diagonal{ radius * ImSqrt( 0.5f ) };
        const float quarterTurn{ IM_PI * 0.25f };

        draw.PathLineTo( { points[1].x, points[1].y - diagonal * 2.0f } );
        draw.PathArcTo( points[2], radius, -quarterTurn * 3.0f, quarterTurn );
        draw.PathArcTo( points[1], radius, quarterTurn, quarterTurn * 3.0f );
        draw.PathArcTo( points[0], radius, quarterTurn * 3.0f, quarterTurn * 7.0f );
        draw.PathFillConcave( color );
    }

    auto PushImageButton( eastl::string_view ID, ImTextureID textureHandle, ImVec2 size ) -> bool {
        MKT_BEGIN_PROFILER();

        // Texture previews need an even inset rather than text-button padding.
        // ImageButton derives the inner corner radius from this inset and the
        // frame radius. Draw one frame, without a second border/shadow fringe.
        const ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 2.0f, 2.0f } };
        const ImGuiScopedStyleVar border{ ImGuiStyleVar_FrameBorderSize, 0.0f };
        return ImGui::ImageButton( ID.data(), textureHandle, size );
    }

    auto PushImageButton( core::u64 textureId, ImTextureID textureHandle, ImVec2 size ) -> bool {
        MKT_BEGIN_PROFILER();
        return PushImageButton( string::Format( "{}", textureId ), textureHandle, size );
    }

    auto CheckBox( eastl::string_view label, bool& value ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( ImGui::GetCurrentWindow()->SkipItems ) {
            return false;
        }

        // Inherit the shared frame height used by inputs, dropdowns and buttons.
        // A single rounded fill avoids overlapping border fringes at the corners.
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 0.0f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 5.0f };

        // Checked and unchecked boxes share the dark fill; hover only lifts it slightly.
        // https://github.com/ocornut/imgui/blob/master/imgui_widgets.cpp
        ImGuiScopedColor background{ ImGuiCol_FrameBg, ImVec4{ 0.045f, 0.045f, 0.045f, 1.0f } };
        ImGuiScopedColor selected{ ImGuiCol_CheckboxSelectedBg, ImVec4{ 0.045f, 0.045f, 0.045f, 1.0f } };
        ImGuiScopedColor hovered{ ImGuiCol_FrameBgHovered, ImVec4{ 0.10f, 0.10f, 0.10f, 1.0f } };
        ImGuiScopedColor pressed{ ImGuiCol_FrameBgActive, ImVec4{ 0.08f, 0.08f, 0.08f, 1.0f } };
        const ImU32 checkColor{ ImGui::GetColorU32( ImVec4{ 232.0f / 255.0f, 191.0f / 255.0f, 124.0f / 255.0f, 1.0f } ) };
        const float squareSize{ ImGui::GetFrameHeight() };

        // Keep native behavior and replace only the mark's rendering, including mixed values.
        ImGuiScopedColor hideNativeMark{ ImGuiCol_CheckMark, ImVec4{ 0, 0, 0, 0 } };
        const bool active{ ImGui::Checkbox( label.data(), MKT_ADDRESSOF( value ) ) };

        if ( ImGui::IsItemVisible() ) {
            auto* draw{ ImGui::GetWindowDrawList() };
            const auto origin{ ImGui::GetItemRectMin() };

            if ( ImGui::GetItemFlags() & ImGuiItemFlags_MixedValue ) {
                const float padding{ ImMax( 1.0f, ImFloor( squareSize / 3.6f ) ) };
                draw->AddRectFilled( { origin.x + padding, origin.y + padding },
                    { origin.x + squareSize - padding, origin.y + squareSize - padding }, checkColor, 2.0f );
            }
            else if ( value ) {
                const float padding{ ImMax( 1.0f, ImFloor( squareSize / 6.0f ) ) };
                DrawRoundedCheckMark( *draw, { origin.x + padding, origin.y + padding }, checkColor, squareSize - padding * 2.0f );
            }
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto DragFloat4( eastl::string_view label, eastl::string_view format, glm::vec4& vect, float speed, float minVal, float maxVal ) -> bool {
        MKT_BEGIN_PROFILER();
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat4( label.data(), value_ptr( vect ), speed, minVal, maxVal, format.data() ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto DragFloat3( eastl::string_view label, eastl::string_view format, glm::vec3& vect, float speed, float minVal, float maxVal ) -> bool {
        MKT_BEGIN_PROFILER();
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat3( label.data(), value_ptr( vect ), speed, minVal, maxVal, format.data() ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto DragFloat3( eastl::string_view label, core::c_str format, glm::vec3& vect, float speed, float minVal, float maxVal ) -> bool {
        MKT_BEGIN_PROFILER();
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat3( label.data(), value_ptr( vect ), speed, minVal, maxVal, format ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto ColorEdit4( eastl::string_view label, glm::vec4& vect ) -> bool {
        MKT_BEGIN_PROFILER();
        constexpr ImGuiColorEditFlags colorEditFlags{
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar
        };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        const bool active{ ImGui::ColorEdit4( label.data(), value_ptr( vect ), colorEditFlags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto ColorEdit3( eastl::string_view label, glm::vec3& vect ) -> bool {
        MKT_BEGIN_PROFILER();
        constexpr ImGuiColorEditFlags colorEditFlags{
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar
        };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        const bool active{ ImGui::ColorEdit3( label.data(), value_ptr( vect ), colorEditFlags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto Slider( eastl::string_view label, float& value, const glm::vec2& bounds, eastl::string_view format ) -> bool {
        MKT_BEGIN_PROFILER();
        constexpr ImGuiSliderFlags flags{ ImGuiSliderFlags_None };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        const bool active{ ImGui::SliderFloat( label.data(), std::addressof( value ), bounds.x, bounds.y, format.data(), flags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto ButtonTextIcon( eastl::string_view icon, ImVec2 size ) -> bool {
        MKT_BEGIN_PROFILER();
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 0.0f, 0.0f } };

        const bool active{ ImGui::Button( string::Format( "{}", icon ).c_str(), size ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    auto TextArea( eastl::string& buffer ) -> bool {
        MKT_BEGIN_PROFILER();
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        auto resizeCallback = []( ImGuiInputTextCallbackData* data ) -> int {
            if ( data->EventFlag == ImGuiInputTextFlags_CallbackResize ) {
                auto* str{ static_cast<eastl::string*>( data->UserData ) };
                str->resize( data->BufTextLen );
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
            MKT_ADDRESSOF( buffer ) ) };

        // ImGui also edits in place when the allocation is large enough; the
        // resize callback alone does not update EASTL's logical string length.
        if ( active ) {
            buffer.force_size( eastl::CharStrlen( buffer.c_str() ) );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_TextInput );
        }

        return active;
    }

    auto Combo( eastl::string* choices, core::usize count, const eastl::string& currentSelection ) -> core::i32 {
        MKT_BEGIN_PROFILER();
        core::i32 selectionIndex{ -1 };

        const eastl::string labelName{
            string::Format( "##{}{}", __PRETTY_FUNCTION__, currentSelection )
        };

        if ( ImGui::BeginCombo( labelName.data(), currentSelection.c_str() ) ) {
            for ( core::usize index{}; index < count; ++index ) {
                const eastl::string& selectionStr{ choices[index] };

                const bool isSelected{ selectionStr == currentSelection };

                if ( ImGui::Selectable(
                         fmt::format( " {}", selectionStr ).c_str(),
                         isSelected ) ) {
                    selectionIndex = core::as<core::i32>( index );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return selectionIndex;
    }

    auto InputText( eastl::string_view viewData, bool readOnly ) -> bool {
        MKT_BEGIN_PROFILER();
        ImGuiTextFlags flags{ ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll };

        if ( readOnly ) {
            flags |= ImGuiInputTextFlags_ReadOnly;
        }

        constexpr core::u32 kLength{ 1024 };
        eastl::array<char, kLength> name{};
        std::ranges::copy( viewData, name.data() );

        return ImGui::InputText( "##DrawNameTextInputTag", name.data(), name.max_size(), flags );
    }

    auto InputText( eastl::string& viewData, ImGuiTextFlags flags ) -> bool {
        MKT_BEGIN_PROFILER();

        constexpr core::u32 kLength{ 1024 };
        eastl::array<char, kLength> name{};
        std::ranges::copy( viewData, name.data() );

        bool v{ ImGui::InputText( "##DrawNameTextInputTag", name.data(), name.max_size(), flags ) };

        if ( v ) {
            viewData = name.data();
        }

        return v;
    }

    auto widget::LocalizedLabel( core::cstr key, core::cstr stableId ) -> eastl::string {
        eastl::string label{ MKT_LOC( key ) };
        label += "###";
        label += stableId ? stableId : key;
        return label;
    }

    auto widget::BeginItemTooltip( float delay ) -> bool {
        auto &style{ ImGui::GetStyle() };
        const float savedDelay{ style.HoverDelayNormal };
        const float wait{ delay < 0.0f ? savedDelay : delay };
        // Keep ImGui's per-item hover timer active even with a zero-delay preference.
        style.HoverDelayNormal = ImMax( wait, 0.0001f );
        const bool hovered{ ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNormal |
                                                  ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_AllowWhenDisabled ) };
        style.HoverDelayNormal = savedDelay;

        if ( !hovered ) {
            return false;
        }

        // Continue tracking hover after expiry, so it only reappears on a fresh hover.
        // ImGui's timer also supports text and other items without an explicit ID.
        constexpr float displayDuration{ 5.0f };
        constexpr float fadeDuration{ 0.15f };
        const float elapsed{ ImGui::GetCurrentContext()->HoverItemDelayTimer - wait };

        if ( elapsed >= displayDuration ) {
            return false;
        }

        const float alpha{ ImClamp( ImMin( elapsed, displayDuration - elapsed ) / fadeDuration, 0.0f, 1.0f ) };
        ImGui::PushStyleVar( ImGuiStyleVar_Alpha, style.Alpha * alpha );
        // Toolbars may remove window padding; their tooltips still need a text inset.
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding,
            ImVec2{ ImMax( style.WindowPadding.x, 12.0f ), ImMax( style.WindowPadding.y, 8.0f ) } );

        if ( !ImGui::BeginTooltip() ) {
            ImGui::PopStyleVar( 2 );
            return false;
        }

        ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
        return true;
    }

    auto widget::EndItemTooltip() -> void {
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        ImGui::PopStyleVar( 2 );
    }

    auto widget::ItemTooltip( core::cstr format, ... ) -> void {
        if ( BeginItemTooltip() ) {
            va_list arguments;
            va_start( arguments, format );
            ImGui::TextV( format, arguments );
            va_end( arguments );
            EndItemTooltip();
        }
    }

    auto widget::TooltipText( eastl::string_view text, float delay ) -> void {
        if ( BeginItemTooltip( delay ) ) {
            ImGui::TextUnformatted( text.data(), text.data() + text.size() );
            EndItemTooltip();
        }
    }

    auto widget::MakeIconTitle( eastl::string_view panelIcon, eastl::string_view panelName ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{} {}", panelIcon.data(), panelName.data() );
    }

    auto widget::MakeHelpPopUp( eastl::string_view description, eastl::string_view placeHolder ) -> void {
        if ( !placeHolder.empty() ) {
            ImGui::TextDisabled( "%.*s", core::as<int>( placeHolder.size() ), placeHolder.data() );
        }

        TooltipText( description );
    }

    auto widget::MakeHelpPopUpDelay( eastl::string_view description, eastl::string_view placeHolder, float duration ) -> void {
        if ( !placeHolder.empty() ) {
            ImGui::TextDisabled( "%.*s", core::as<int>( placeHolder.size() ), placeHolder.data() );
        }

        TooltipText( description, duration );
    }

    auto widget::SearchFilter( core::cstr id, ImGuiTextFilter& filter, eastl::string_view hint, float width ) -> bool {
        const auto placeholder{ string::Format( "{} {}", ICON_MD_SEARCH, hint ) };
        ImGui::SetNextItemWidth( width );

        if ( !ImGui::InputTextWithHint( id, placeholder.c_str(), filter.InputBuf, IM_ARRAYSIZE( filter.InputBuf ) ) ) {
            return false;
        }

        filter.Build();
        return true;
    }

    auto widget::IconButton( core::cstr id, core::cstr icon, ImVec2 size ) -> bool {
        ImGui::PushID( id );
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 0.0f, 0.0f } );
        const bool clicked{ ImGui::Button( "##Icon", size ) };
        ImGui::PopStyleVar();

        if ( ImGui::IsItemVisible() ) {
            unsigned int codepoint{};
            ImTextCharFromUtf8( &codepoint, icon, nullptr );
            auto* baked{ ImGui::GetFontBaked() };
            const auto* glyph{ baked->FindGlyph( static_cast<ImWchar>( codepoint ) ) };

            if ( glyph && glyph->Visible ) {
                const auto min{ ImGui::GetItemRectMin() };
                const auto max{ ImGui::GetItemRectMax() };
                const float scale{ ImGui::GetFontSize() / baked->Size };
                const ImVec2 origin{
                    ( min.x + max.x - ( glyph->X0 + glyph->X1 ) * scale ) * 0.5f,
                    ( min.y + max.y - ( glyph->Y0 + glyph->Y1 ) * scale ) * 0.5f
                };
                // Glyph bounds include the merged icon font's baseline offset.
                ImGui::GetWindowDrawList()->AddText( origin, ImGui::GetColorU32( ImGuiCol_Text ), icon );
            }
        }

        ImGui::PopID();
        return clicked;
    }

    auto widget::ToggleIconButton( core::cstr id, core::cstr icon, bool selected, ImVec2 size ) -> bool {
        if ( selected ) {
            const auto color{ ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) };
            ImGui::PushStyleColor( ImGuiCol_Button, color );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, color );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, color );
        }

        const bool clicked{ IconButton( id, icon, size ) };

        if ( selected ) {
            ImGui::PopStyleColor( 3 );
        }

        SetCursorHandOnLastItemHovered();
        return clicked;
    }

    auto widget::FadingImageButton( core::cstr id, ImTextureRef texture, ImVec2 imageSize,
        ImVec2 uv0, ImVec2 uv1, ImVec4 background, ImVec4 tint, float fadeDuration ) -> bool {
        auto* window{ ImGui::GetCurrentWindow() };

        if ( window->SkipItems ) {
            return false;
        }

        const auto& style{ ImGui::GetStyle() };
        const auto padding{ style.FramePadding };
        const auto minimum{ ImGui::GetCursorScreenPos() };
        const ImRect bounds{ minimum, { minimum.x + imageSize.x + padding.x * 2.0f, minimum.y + imageSize.y + padding.y * 2.0f } };
        const ImGuiID buttonID{ window->GetID( id ) };
        ImGui::ItemSize( bounds );

        if ( !ImGui::ItemAdd( bounds, buttonID ) ) {
            return false;
        }

        bool hovered{}, held{};
        const bool pressed{ ImGui::ButtonBehavior( bounds, buttonID, &hovered, &held ) };
        const auto& context{ *ImGui::GetCurrentContext() };
        const bool disabled{ ( context.LastItemData.ItemFlags & ImGuiItemFlags_Disabled ) != 0 };
        const bool keyboardFocus{ context.NavCursorVisible && ImGui::IsItemFocused() };
        const float target{ !disabled && ( hovered || held || keyboardFocus ) ? 1.0f : 0.0f };

        // Window-owned state follows the scoped button ID, not a transient texture descriptor.
        auto* storage{ ImGui::GetStateStorage() };
        const ImGuiID fadeID{ ImHashStr( "ImageButtonFade", 0, buttonID ) };
        const ImGuiID frameID{ ImHashStr( "ImageButtonFadeFrame", 0, buttonID ) };
        const int frame{ ImGui::GetFrameCount() };
        const int previousFrame{ storage->GetInt( frameID, -2 ) };
        float fade{ previousFrame == frame - 1 || previousFrame == frame ? storage->GetFloat( fadeID ) : 0.0f };

        if ( previousFrame != frame ) {
            const float step{ fadeDuration > 0.0f ? ImGui::GetIO().DeltaTime / fadeDuration : 1.0f };
            fade = target > fade ? ImMin( target, fade + step ) : ImMax( target, fade - step );
            storage->SetFloat( fadeID, fade );
            storage->SetInt( frameID, frame );
        }

        auto frameColor{ style.Colors[held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered] };
        frameColor.w *= fade;
        ImGui::RenderFrame( bounds.Min, bounds.Max, ImGui::GetColorU32( frameColor ), false, style.FrameRounding );
        const ImVec2 imageMin{ bounds.Min.x + padding.x, bounds.Min.y + padding.y };
        const ImVec2 imageMax{ bounds.Max.x - padding.x, bounds.Max.y - padding.y };

        if ( background.w > 0.0f ) {
            window->DrawList->AddRectFilled( imageMin, imageMax, ImGui::GetColorU32( background ) );
        }

        const float rounding{ ImMax( style.FrameRounding - ImMax( padding.x, padding.y ), style.ImageRounding ) };
        window->DrawList->AddImageRounded( texture, imageMin, imageMax, uv0, uv1, ImGui::GetColorU32( tint ), rounding );
        ImGui::RenderNavCursor( bounds, buttonID );
        return pressed;
    }

    auto widget::ToolbarIconButton( core::cstr id, core::cstr icon, core::cstr tooltip ) -> bool {
        const float extent{ ImGui::GetFrameHeight() + 4.0f };
        return ToolbarButton( id, icon, tooltip, { extent, extent } );
    }

    auto widget::ToolbarButton( core::cstr id, core::cstr label, core::cstr tooltip, ImVec2 size ) -> bool {
        ImGui::PushID( id );
        // Flat toolbar treatment: no border, shadow or resting background.
        // Low-opacity theme text provides a subtle, readable hover/press tint.
        const ImVec4 tint{ ImGui::GetStyleColorVec4( ImGuiCol_Text ) };
        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 3.0f );
        ImGui::PushStyleColor( ImGuiCol_Button, { 0, 0, 0, 0 } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { tint.x, tint.y, tint.z, 0.10f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, { tint.x, tint.y, tint.z, 0.18f } );
        const bool clicked{ ImGui::Button( label, size ) };
        ImGui::PopStyleColor( 3 );
        ImGui::PopStyleVar( 2 );
        SetCursorHandOnLastItemHovered();

        TooltipText( tooltip );

        ImGui::PopID();
        return clicked;
    }

    auto widget::AccentButton( core::cstr label, ImVec2 size ) -> bool {
        ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_Header ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4( ImGuiCol_HeaderHovered ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4( ImGuiCol_HeaderActive ) );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
        const bool pressed{ ImGui::Button( label, size ) };
        ImGui::PopStyleVar();
        ImGui::PopStyleColor( 3 );
        return pressed;
    }

    auto widget::CircleInt( eastl::string_view id, core::i32* value, core::i32 min, core::i32 max ) -> bool {
        if ( value == nullptr || min >= max ) {
            return false;
        }

        constexpr float radius{ 8.0f };
        constexpr float trackHeight{ 4.0f };
        constexpr float inputWidth{ 80.0f };
        constexpr float labelWidth{ 110.0f };
        constexpr float rightPadding{ 8.0f };

        const bool showLabel{ id.find( '#' ) == eastl::string_view::npos };
        const bool showInput{ id.find( '#' ) != eastl::string_view::npos };

        const int columns{
            1 + static_cast<int>( showLabel ) + static_cast<int>( showInput )
        };

        ImGui::PushID( id.data(), id.data() + id.size() );

        bool changed{ false };

        if ( ImGui::BeginTable(
                 "##CircleIntTable",
                 columns,
                 ImGuiTableFlags_SizingStretchProp |
                     ImGuiTableFlags_NoSavedSettings ) ) {
            if ( showLabel ) {
                ImGui::TableSetupColumn(
                    "Label",
                    ImGuiTableColumnFlags_WidthFixed,
                    labelWidth );
            }

            ImGui::TableSetupColumn(
                "Slider",
                ImGuiTableColumnFlags_WidthStretch,
                1.0f );

            if ( showInput ) {
                ImGui::TableSetupColumn(
                    "Input",
                    ImGuiTableColumnFlags_WidthFixed,
                    inputWidth );
            }

            ImGui::TableNextRow();

            // Optional label
            if ( showLabel ) {
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( id.data(), id.data() + id.size() );
            }

            // Slider
            ImGui::TableNextColumn();

            const float width{
                std::max(
                    radius * 2.0f + 1.0f,
                    ImGui::GetContentRegionAvail().x -
                        ( showInput ? 0.0f : rightPadding ) )
            };

            const float height{ ImGui::GetFrameHeight() };
            const ImVec2 pos{ ImGui::GetCursorScreenPos() };

            ImGui::InvisibleButton( "##slider", ImVec2{ width, height } );

            const float x0{ pos.x + radius };
            const float x1{ pos.x + width - radius };
            const float cy{ pos.y + height * 0.5f };

            const bool active{ ImGui::IsItemActive() };

            if ( active && ImGui::IsMouseDown( 0 ) ) {
                const float t{ std::clamp(
                    ( ImGui::GetIO().MousePos.x - x0 ) / ( x1 - x0 ),
                    0.0f, 1.0f ) };

                const core::i32 next{
                    static_cast<core::i32>( std::round(
                        std::lerp(
                            static_cast<double>( min ),
                            static_cast<double>( max ),
                            static_cast<double>( t ) ) ) )
                };

                changed = next != *value;
                *value = next;
            }

            const float t{ std::clamp(
                static_cast<float>(
                    ( static_cast<double>( *value ) - min ) /
                    ( static_cast<double>( max ) - min ) ),
                0.0f, 1.0f ) };

            const float knobX{ std::lerp( x0, x1, t ) };

            ImDrawList* draw{ ImGui::GetWindowDrawList() };

            // Background track
            draw->AddRectFilled(
                ImVec2{ x0, cy - trackHeight * 0.5f },
                ImVec2{ x1, cy + trackHeight * 0.5f },
                ImGui::GetColorU32( ImGuiCol_FrameBg ),
                trackHeight * 0.5f );

            // Filled track
            if ( knobX > x0 ) {
                draw->AddRectFilled(
                    ImVec2{ x0, cy - trackHeight * 0.5f },
                    ImVec2{ knobX, cy + trackHeight * 0.5f },
                    ImGui::GetColorU32( ImGuiCol_SliderGrab ),
                    trackHeight * 0.5f );
            }

            // Circular handle
            draw->AddCircleFilled(
                ImVec2{ knobX, cy },
                radius,
                ImGui::GetColorU32(
                    active
                        ? ImGuiCol_SliderGrabActive
                        : ImGuiCol_SliderGrab ) );

            // Optional integer input
            if ( showInput ) {
                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(
                    std::max(
                        1.0f,
                        ImGui::GetContentRegionAvail().x - rightPadding ) );

                if ( ImGui::InputInt( "##input", value, 0, 0 ) ) {
                    *value = std::clamp( *value, min, max );
                    changed = true;
                }
            }

            ImGui::EndTable();
        }

        ImGui::PopID();
        return changed;
    }

    auto widget::CircleSlider( eastl::string_view id, core::f32* value, core::f32 min, core::f32 max, eastl::string_view format, bool clampInput ) -> bool {
        if ( value == nullptr || min >= max ) {
            return false;
        }

        constexpr float radius{ 8.0f };
        constexpr float trackHeight{ 4.0f };
        constexpr float inputWidth{ 80.0f };
        constexpr float labelWidth{ 110.0f };

        // Match ImGui labels: display the text before ##, retaining the full ID.
        const auto label{ id.substr( 0, id.find( "##" ) ) };
        const bool showLabel{ !label.empty() };
        const bool showInput{ !format.empty() };

        const int columns{
            1 + static_cast<int>( showLabel ) + static_cast<int>( showInput )
        };

        ImGui::PushID( id.data(), id.data() + id.size() );

        bool changed{ false };

        if ( ImGui::BeginTable(
                 "##CircleSliderTable",
                 columns,
                 ImGuiTableFlags_SizingStretchProp |
                     ImGuiTableFlags_NoSavedSettings ) ) {
            if ( showLabel ) {
                ImGui::TableSetupColumn(
                    "Label",
                    ImGuiTableColumnFlags_WidthFixed,
                    labelWidth );
            }

            ImGui::TableSetupColumn(
                "Slider",
                ImGuiTableColumnFlags_WidthStretch,
                1.0f );

            if ( showInput ) {
                ImGui::TableSetupColumn(
                    "Input",
                    ImGuiTableColumnFlags_WidthFixed,
                    inputWidth );
            }

            ImGui::TableNextRow();

            // Optional label column
            if ( showLabel ) {
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( label.data(), label.data() + label.size() );
            }

            // Slider column
            ImGui::TableNextColumn();

            const float width{
                std::max(
                    radius * 2.0f + 1.0f,
                    ImGui::GetContentRegionAvail().x )
            };

            const float height{ ImGui::GetFrameHeight() };
            const ImVec2 pos{ ImGui::GetCursorScreenPos() };

            ImGui::InvisibleButton( "##slider", ImVec2{ width, height } );

            const float x0{ pos.x + radius };
            const float x1{ pos.x + width - radius };
            const float cy{ pos.y + height * 0.5f };

            const bool active{ ImGui::IsItemActive() };

            if ( active && ImGui::IsMouseDown( 0 ) ) {
                const float t{ std::clamp(
                    ( ImGui::GetIO().MousePos.x - x0 ) / ( x1 - x0 ),
                    0.0f, 1.0f ) };

                const float next{ std::lerp( min, max, t ) };
                changed = next != *value;
                *value = next;
            }

            const float t{ std::clamp(
                ( *value - min ) / ( max - min ),
                0.0f, 1.0f ) };

            const float knobX{ std::lerp( x0, x1, t ) };

            ImDrawList* draw{ ImGui::GetWindowDrawList() };

            // Background track
            draw->AddRectFilled(
                ImVec2{ x0, cy - trackHeight * 0.5f },
                ImVec2{ x1, cy + trackHeight * 0.5f },
                ImGui::GetColorU32( ImGuiCol_FrameBg ),
                trackHeight * 0.5f );

            // Filled track
            if ( knobX > x0 ) {
                draw->AddRectFilled(
                    ImVec2{ x0, cy - trackHeight * 0.5f },
                    ImVec2{ knobX, cy + trackHeight * 0.5f },
                    ImGui::GetColorU32( ImGuiCol_SliderGrab ),
                    trackHeight * 0.5f );
            }

            // Circular handle
            draw->AddCircleFilled(
                ImVec2{ knobX, cy },
                radius,
                ImGui::GetColorU32(
                    active
                        ? ImGuiCol_SliderGrabActive
                        : ImGuiCol_SliderGrab ) );

            // Optional numeric input column
            if ( showInput ) {
                ImGui::TableNextColumn();
                constexpr float rightPadding{ 8.0f };
                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - rightPadding );

                const eastl::string fmt{ format.data(), format.size() };

                if ( ImGui::InputFloat(
                         "##input",
                         value,
                         0.0f,
                         0.0f,
                         fmt.c_str() ) ) {
                    if ( clampInput ) {
                        *value = std::clamp( *value, min, max );
                    }

                    changed = true;
                }
            }

            ImGui::EndTable();
        }

        ImGui::PopID();
        return changed;
    }

    auto widget::ToggleButton(
        core::cstr label,
        bool state,
        bool handOnHover,
        ImVec2 size,
        core::f32 alpha,
        core::f32 pressedAlpha,
        ImGuiButtonFlags buttonFlags,
        ImGuiCol activeColor ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( state ) {
            ImVec4 color{ ImGui::GetStyle().Colors[activeColor] };

            color.w = pressedAlpha;
            ImGui::PushStyleColor( ImGuiCol_Button, color );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, color );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, color );
        }
        else {
            ImVec4 color{ ImGui::GetStyle().Colors[ImGuiCol_Button] };
            ImVec4 hoveredColor{ ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] };
            color.w = alpha;
            hoveredColor.w = pressedAlpha;
            ImGui::PushStyleColor( ImGuiCol_Button, color );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, hoveredColor );
            color.w = pressedAlpha;
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, color );
        }

        const bool clicked{ ImGui::ButtonEx( label, size, buttonFlags ) };

        if ( handOnHover ) {
            SetCursorHandOnLastItemHovered();
        }

        ImGui::PopStyleColor( 3 );

        return clicked;
    }

}// namespace mikoto::imgui
