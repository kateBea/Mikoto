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

#ifndef MIKOTO_IMGUI_IMGUI_WIDGET_HH
#define MIKOTO_IMGUI_IMGUI_WIDGET_HH

#include <EASTL/span.h>

#include <glm/gtc/type_ptr.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <ImGui/Utility.hh>

namespace mikoto::imgui {

    /**
     * @brief Draws an image button with an explicit interaction identifier.
     * @param textureId Identifier used to distinguish this image button.
     * @param textureHandle Texture descriptor displayed by the image button.
     * @param size Widget dimensions in pixels; zero uses the default size.
     * @returns True when activated.
     */
    auto PushImageButton( core::u64 textureId, ImTextureID textureHandle, ImVec2 size ) -> bool;

    /**
     * @brief Draws an image button with an explicit interaction identifier.
     * @param ID Stable interaction identifier.
     * @param textureHandle Texture descriptor displayed by the image button.
     * @param size Widget dimensions in pixels; zero uses the default size.
     * @returns True when activated.
     */
    auto PushImageButton( eastl::string_view ID, ImTextureID textureHandle, ImVec2 size ) -> bool;

    /**
     * @brief Draws a checkbox and writes its edited value back.
     * @param label Visible widget label and interaction identifier.
     * @param value Value to inspect or edit.
     * @returns True when the value changes.
     */
    auto CheckBox( eastl::string_view label, bool &value ) -> bool;

    /**
     * @brief Draws draggable controls for four scalar components.
     * @param label Visible widget label and interaction identifier.
     * @param format Formatting or storage format selected by the caller.
     * @param[in,out] vect Vector or color components edited by the widget.
     * @param speed Drag sensitivity in value units per mouse movement.
     * @param minVal Lower component limit.
     * @param maxVal Upper component limit.
     * @returns True when a component changes.
     */
    auto DragFloat4( eastl::string_view label, eastl::string_view format, glm::vec4 &vect, float speed, float minVal, float maxVal ) -> bool;

    /**
     * @brief Draws draggable controls for three scalar components.
     * @param label Visible widget label and interaction identifier.
     * @param format Formatting or storage format selected by the caller.
     * @param[in,out] vect Vector or color components edited by the widget.
     * @param speed Drag sensitivity in value units per mouse movement.
     * @param minVal Lower component limit.
     * @param maxVal Upper component limit.
     * @returns True when a component changes.
     */
    auto DragFloat3( eastl::string_view label, eastl::string_view format, glm::vec3 &vect, float speed, float minVal, float maxVal ) -> bool;

    /**
     * @brief Draws an RGBA color editor.
     * @param label Visible widget label and interaction identifier.
     * @param[in,out] vect Vector or color components edited by the widget.
     * @returns True when a color component changes.
     */
    auto ColorEdit4( eastl::string_view label, glm::vec4 &vect ) -> bool;

    /**
     * @brief Draws an RGB color editor.
     * @param label Visible widget label and interaction identifier.
     * @param[in,out] vect Vector or color components edited by the widget.
     * @returns True when a color component changes.
     */
    auto ColorEdit3( eastl::string_view label, glm::vec3 &vect ) -> bool;

    /**
     * @brief Draws a scalar slider within the supplied bounds.
     * @param label Visible widget label and interaction identifier.
     * @param value Value to inspect or edit.
     * @param bounds Minimum and maximum slider values.
     * @param format Formatting or storage format selected by the caller.
     * @returns True when the value changes.
     */
    auto Slider( eastl::string_view label, float &value, const glm::vec2 &bounds, eastl::string_view format = "%.2f" ) -> bool;

    /**
     * @brief Draws a button labeled with an icon glyph.
     * @param icon UTF-8 icon glyph used as the visible label.
     * @param size Widget dimensions in pixels; zero uses the default size.
     * @returns True when activated.
     */
    auto ButtonTextIcon( eastl::string_view icon, ImVec2 size = { 0.0f, 0.0f } ) -> bool;

    /**
     * @brief Draws a text area backed by an editable string.
     * @param[in,out] buffer Text storage edited by the widget.
     * @returns True when edited text is written back.
     */
    auto TextArea( eastl::string &buffer ) -> bool;

    /**
     * @brief Draws a combo box for a set of choices.
     * @param choices Selectable labels or values.
     * @param count Number of selectable labels in choices.
     * @param currentSelection Current selection used as the preview.
     * @returns Chosen item index, or -1 when no new selection is made.
     */
    MKT_NODISCARD auto Combo(eastl::string* choices, core::usize count, const eastl::string& currentSelection) -> core::i32;

    /**
     * @brief Draws a text input using the supplied value and flags.
     * @param viewData Input text; must contain fewer than 1024 bytes.
     * @param readOnly Whether editing is disabled; the string-view overload always edits a temporary copy.
     * @returns Underlying ImGui input result, according to its flags.
     */
    auto InputText(eastl::string_view viewData, bool readOnly = false) -> bool;

    /**
     * @brief Draws a text input using the supplied value and flags.
     * @param viewData Input text; must contain fewer than 1024 bytes.
     * @param flags ImGui text-input behavior flags.
     * @returns Underlying ImGui input result, according to its flags.
     */
    auto InputText(eastl::string& viewData, ImGuiTextFlags flags ) -> bool;

    /**
     * @brief Draws a range-backed combo and updates the active selection.
     * @tparam InputIt Iterator over the selectable labels.
     * @tparam Pred Predicate testing whether an item is selected.
     * @param start Iterator to the first selectable item.
     * @param end One-past-last iterator of the borrowed range.
     * @param[in,out] currentlyActive Text updated when a choice is selected.
     * @param isSelectedPred Predicate identifying the selected item in the range.
     * @param label Visible widget label and interaction identifier.
     */
    template<typename InputIt, typename Pred>
    auto ComboList( InputIt start, InputIt end, eastl::string &currentlyActive, Pred &&isSelectedPred, const eastl::string_view label ) -> void {
        MKT_BEGIN_PROFILER();
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar popUpRounding{ ImGuiStyleVar_PopupRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f, 10.0f } };

        constexpr ImGuiComboFlags comboFlags{ ImGuiComboFlags_None };
        constexpr ImGuiSelectableFlags selectableFlags{ ImGuiSelectableFlags_None };

        if (ImGui::BeginCombo( fmt::format( "##{}", label ).c_str(), currentlyActive.c_str(), comboFlags )) {
            for (; start != end; ++start) {
                const bool isSelected{ isSelectedPred( *start ) };

                if ( ImGui::Selectable( fmt::format( " {}", *start ).c_str(), isSelected, selectableFlags ) ) {
                    currentlyActive = *start;
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
    }

    /**
     * @brief Draws a collapsible node and invokes its contents while open.
     * @tparam UIFunction Callable drawing the expanded node contents.
     * @tparam Args Types of the forwarded argument pack.
     * @param label Visible widget label and interaction identifier.
     * @param uiFunc Callback drawing the node contents while expanded.
     * @param args Additional values forwarded to the operation.
     */
    template<typename UIFunction, typename... Args>
    auto DrawNode( const std::string_view label, const UIFunction& uiFunc, Args&&... args ) -> void {
        MKT_BEGIN_PROFILER();
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };

        const std::string panelName{ fmt::format( "{}{}", __PRETTY_FUNCTION__, label ) };
        const bool componentNodeOpen{
            ImGui::TreeNodeEx( reinterpret_cast<const void*>( label.data() ), treeNodeFlags, "%s", label.data() )
        };

        // Node frame is hovered
        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        if ( componentNodeOpen ) {

            uiFunc( eastl::forward<Args>( args )... );

            ImGui::TreePop();
        }
    }

    // TODO: Use enum reflection for this
    /**
     * @brief Draws a combo box for a set of choices.
     * @tparam EnumType Enumeration whose values select entries in choices.
     * @param choices Selectable labels or values.
     * @param currentSelection Current selection used as the preview.
     * @returns Chosen enumeration value, or currentSelection when no new selection is made.
     */
    template<typename EnumType>
    MKT_NODISCARD auto Combo(std::span<std::string> choices, EnumType currentSelection) -> EnumType {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( static_cast<core::u32>( currentSelection ) < choices.size(), "Enum value must be lower than choices size" );

        EnumType result{ currentSelection };

        const std::string &currentChoiceStr{ choices[static_cast<core::u32>( currentSelection )] };
        const std::string labelName{ fmt::format( "##{}{}", __PRETTY_FUNCTION__, currentChoiceStr ) };

        if ( ImGui::BeginCombo( labelName.data(), currentChoiceStr.c_str() ) ) {
            core::u32 selectionIndex{};

            for ( const std::string &selectionStr: choices ) {
                const bool isSelected{ selectionStr == choices[static_cast<core::u32>( currentSelection )] };

                if ( ImGui::Selectable( fmt::format( " {}", selectionStr ).c_str(), isSelected ) ) {
                    result = static_cast<EnumType>( selectionIndex );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }

                ++selectionIndex;
            }

            ImGui::EndCombo();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return result;
    }

    namespace widget {

        /**
         * @brief Translates a widget label while preserving its interaction ID.
         *
         * @param key Localization key, also used as the stable ImGui ID.
         * @param stableId Optional identifier that remains fixed when the language changes.
         * @return Translated label followed by an invisible, language-independent ID.
         */
        MKT_NODISCARD auto LocalizedLabel( core::cstr key, core::cstr stableId = nullptr ) -> eastl::string;

        /**
         * @brief Shows a hover tooltip for five seconds, with a short fade in and out.
         * @param delay Hover delay in seconds; a negative value uses the Preferences setting.
         * @return True when open; pair only a successful call with EndItemTooltip().
         * After expiry, moving away and hovering again starts a new display period.
         */
        auto BeginItemTooltip( float delay = -1.0f ) -> bool;

        /**
         * @brief Closes a successfully opened item tooltip.
         */
        auto EndItemTooltip() -> void;

        /**
         * @brief Displays a formatted tooltip for the last item.
         * @param format Formatting or storage format selected by the caller.
         * @param ... Arguments corresponding to the printf-style format string.
         */
        auto ItemTooltip( core::cstr format, ... ) -> void;

        /**
         * @brief Displays borrowed tooltip text after its hover delay.
         * @param text Borrowed text displayed in the tooltip.
         * @param delay Hover delay in seconds; a negative value uses the Preferences setting.
         */
        auto TooltipText( eastl::string_view text, float delay = -1.0f ) -> void;

        /**
         * @brief Draws a help marker that reveals text on hover.
         * @param description Explanatory text shown by the tooltip.
         * @param placeHolder Visible help-marker label.
         */
        auto MakeHelpPopUp( eastl::string_view description, eastl::string_view placeHolder = "(?)" ) -> void;

        /**
         * @brief Draws a help marker with a configurable hover delay.
         * @param description Explanatory text shown by the tooltip.
         * @param placeHolder Visible help-marker label.
         * @param duration Hover delay in seconds.
         */
        auto MakeHelpPopUpDelay(eastl::string_view description, eastl::string_view placeHolder, float duration = 2.0f ) -> void;

        /**
         * @brief Combines a panel's icon and visible name.
         * @param panelIcon UTF-8 glyph used as the panel icon.
         * @param panelName Visible panel title.
         * @returns Combined panel-title string.
         */
        MKT_NODISCARD auto MakeIconTitle( eastl::string_view panelIcon, eastl::string_view panelName ) -> eastl::string;

        // =======================================================================================================================================
        // Text widgets
        // =======================================================================================================================================
        /**
         * @brief Displays tooltip text or invokes enabled tooltip content.
         * @param description Explanatory text shown by the tooltip.
         */
        auto ToolTip( eastl::string_view description ) -> void;

        /**
         * @brief Displays tooltip text or invokes enabled tooltip content.
         * @param func Callback that draws tooltip content.
         * @param enable Whether tooltip drawing is enabled.
         */
        auto ToolTip( const eastl::function<void()> &func, bool enable ) -> void;

        /**
         * @brief Draws a text filter with a search icon and placeholder inside the input.
         * @param id Stable ImGui identifier; use a hidden label (## prefix).
         * @param filter Filter to edit and rebuild when its text changes.
         * @param hint Localized placeholder shown while the input is empty.
         * @param width Input width; negative values fill the remaining space.
         * @return Whether the filter changed.
         */
        auto SearchFilter( core::cstr id, ImGuiTextFilter& filter, eastl::string_view hint, float width = -1.0f ) -> bool;

        // Icon-only action using theme colors, spacing, and a delayed fading tooltip.
        /**
         * @brief Draws a button with its icon's visible bounds centered in the frame.
         * @param id Stable widget identifier within the current ID scope.
         * @param icon A single UTF-8 icon glyph from the current font.
         * @param size Button dimensions in pixels.
         * @return True when the button is activated.
         */
        auto IconButton( core::cstr id, core::cstr icon, ImVec2 size ) -> bool;

        /**
         * @brief Draws a centered icon with the theme's active button color while selected.
         * @param id Stable widget identifier within the current ID scope.
         * @param icon A single UTF-8 icon glyph from the current font.
         * @param selected Whether the button displays its selected state.
         * @param size Button dimensions in pixels.
         * @returns True when the button is activated.
         */
        auto ToggleIconButton( core::cstr id, core::cstr icon, bool selected, ImVec2 size ) -> bool;

        /**
         * @brief Image button with a transparent resting frame and fading hover feedback.
         * @param id Stable widget identifier within the current ID scope.
         * @param texture Image to display; descriptor changes do not reset the animation.
         * @param imageSize Image dimensions, excluding the current style's frame padding.
         * @param uv0 First texture coordinate.
         * @param uv1 Last texture coordinate.
         * @param background Optional background behind transparent image pixels.
         * @param tint Image tint; the image itself does not fade.
         * @param fadeDuration Seconds for a full fade in or out; zero disables animation.
         * @return True when activated, with standard ImGui button interaction semantics.
         */
        auto FadingImageButton( core::cstr id, ImTextureRef texture, ImVec2 imageSize,
            ImVec2 uv0 = { 0, 0 }, ImVec2 uv1 = { 1, 1 }, ImVec4 background = { 0, 0, 0, 0 },
            ImVec4 tint = { 1, 1, 1, 1 }, float fadeDuration = 0.15f ) -> bool;

        /**
         * @brief Draws a toolbar icon button with a delayed tooltip.
         * @param id Stable interaction identifier.
         * @param icon UTF-8 icon glyph used as the visible label.
         * @param tooltip Explanatory text displayed after hovering.
         * @returns True when activated.
         */
        auto ToolbarIconButton( core::cstr id, core::cstr icon, core::cstr tooltip ) -> bool;

        /**
         * @brief Draws a toolbar button with a delayed tooltip.
         * @param id Stable interaction identifier.
         * @param label Visible widget label and interaction identifier.
         * @param tooltip Explanatory text displayed after hovering.
         * @param size Widget dimensions in pixels; zero uses the default size.
         * @returns True when activated.
         */
        auto ToolbarButton( core::cstr id, core::cstr label, core::cstr tooltip, ImVec2 size = { 0, 0 } ) -> bool;

        // Standard button behavior with the editor selection accent and black text
        // while hovered/pressed. Respects BeginDisabled and keyboard navigation.
        /**
         * @brief Draws a button using the editor selection accent.
         * @param label Visible widget label and interaction identifier.
         * @param size Widget dimensions in pixels; zero uses the default size.
         * @returns True when activated.
         */
        auto AccentButton( core::cstr label, ImVec2 size = { 0, 0 } ) -> bool;

        /**
         * @brief Draws a circular-style integer control.
         * @param id Stable interaction identifier.
         * @param[in,out] value Integer edited by the control.
         * @param min Inclusive lower limit.
         * @param max Inclusive upper limit.
         * @returns True when the integer changes.
         */
        auto CircleInt( eastl::string_view id, core::i32 *value, core::i32 min, core::i32 max ) -> bool;

        // clampInput=false keeps the drag range while allowing unrestricted numeric entry.
        /**
         * @brief Draws a circular-style scalar control.
         * @param id Stable interaction identifier.
         * @param[in,out] value Scalar edited by the control.
         * @param min Inclusive lower limit.
         * @param max Inclusive upper limit.
         * @param format Formatting or storage format selected by the caller.
         * @param clampInput Whether manually typed values must also remain inside the slider limits.
         * @returns True when the value changes.
         */
        auto CircleSlider( eastl::string_view id, core::f32 *value, core::f32 min, core::f32 max, eastl::string_view format = "", bool clampInput = true ) -> bool;

        /**
         * @brief Draws a button representing an existing toggle state.
         * @param label Visible widget label and interaction identifier.
         * @param state Current toggle state; the caller owns and updates it.
         * @param handOnHover Whether hovering selects the hand cursor.
         * @param size Widget dimensions in pixels; zero uses the default size.
         * @param alpha Opacity used in the resting state.
         * @param pressedAlpha Opacity used while pressed.
         * @param buttonFlags Additional ImGui button behavior flags.
         * @param activeColor Theme color used for the active state.
         * @returns True when activated; the caller applies the state change.
         */
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
        // Button widgets
        // =======================================================================================================================================
        /**
         * @brief Draws a text area backed by an editable string.
         * @param[in,out] buffer Text storage edited by the widget.
         * @returns True when edited text is written back.
         */
        auto TextArea( eastl::string &buffer ) -> bool;

        /**
         * @brief Draws a checkbox and writes its edited value back.
         * @param label Visible widget label and interaction identifier.
         * @param value Value to inspect or edit.
         * @returns True when the value changes.
         */
        auto CheckBox( eastl::string_view label, bool &value ) -> bool;

        /**
         * @brief Draws a text input using the supplied value and flags.
         * @param[in,out] data Text storage updated according to the input flags.
         * @param flags ImGui text-input behavior flags.
         * @returns Underlying ImGui input result, according to its flags.
         */
        auto InputText( eastl::string &data, ImGuiTextFlags flags ) -> bool;

        /**
         * @brief Draws a button labeled with an icon glyph.
         * @param icon UTF-8 icon glyph used as the visible label.
         * @param size Widget dimensions in pixels; zero uses the default size.
         * @returns True when activated.
         */
        auto ButtonTextIcon( eastl::string_view icon, ImVec2 size = { 0.0f, 0.0f } ) -> bool;

        /**
         * @brief Draws an image button with an explicit interaction identifier.
         * @param textureId Identifier used to distinguish this image button.
         * @param textureHandle Texture descriptor displayed by the image button.
         * @param size Widget dimensions in pixels; zero uses the default size.
         * @returns True when activated.
         */
        auto PushImageButton( core::u64 textureId, ImTextureID textureHandle, ImVec2 size ) -> bool;

        /**
         * @brief Draws an image button with an explicit interaction identifier.
         * @param ID Stable interaction identifier.
         * @param textureHandle Texture descriptor displayed by the image button.
         * @param size Widget dimensions in pixels; zero uses the default size.
         * @returns True when activated.
         */
        auto PushImageButton( eastl::string_view ID, ImTextureID textureHandle, ImVec2 size ) -> bool;

        // =======================================================================================================================================
        // Drag widgets
        // =======================================================================================================================================
        /**
         * @brief Draws a scalar slider within the supplied bounds.
         * @param label Visible widget label and interaction identifier.
         * @param value Value to inspect or edit.
         * @param bounds Minimum and maximum slider values.
         * @param format Formatting or storage format selected by the caller.
         * @returns True when the value changes.
         */
        auto Slider( eastl::string_view label, core::f32 &value, const core::float2 &bounds, eastl::string_view format = "%.2f" ) -> bool;

        /**
         * @brief Draws draggable controls for four scalar components.
         * @param label Visible widget label and interaction identifier.
         * @param format Formatting or storage format selected by the caller.
         * @param[in,out] vect Vector or color components edited by the widget.
         * @param speed Drag sensitivity in value units per mouse movement.
         * @param minVal Lower component limit.
         * @param maxVal Upper component limit.
         * @returns True when a component changes.
         */
        auto DragFloat4( eastl::string_view label, eastl::string_view format, core::float4 &vect, core::f32 speed, core::f32 minVal, core::f32 maxVal ) -> bool;

        /**
         * @brief Draws draggable controls for three scalar components.
         * @param label Visible widget label and interaction identifier.
         * @param format Formatting or storage format selected by the caller.
         * @param[in,out] vect Vector or color components edited by the widget.
         * @param speed Drag sensitivity in value units per mouse movement.
         * @param minVal Lower component limit.
         * @param maxVal Upper component limit.
         * @returns True when a component changes.
         */
        auto DragFloat3( eastl::string_view label, eastl::string_view format, core::float3 &vect, core::f32 speed, core::f32 minVal, core::f32 maxVal ) -> bool;

        // =======================================================================================================================================
        // Dropdown widgets
        // =======================================================================================================================================
        // Returns the index of the selected item, or -1 if no selection was made
        /**
         * @brief Draws a combo box for a set of choices.
         * @param choices Selectable labels or values.
         * @param currentSelectionIndex Index currently selected in choices.
         * @returns Chosen item index, or -1 when no new selection is made.
         */
        MKT_NODISCARD auto Combo( eastl::span<const eastl::string> choices, core::usize currentSelectionIndex ) -> core::i32;

        // =======================================================================================================================================
        // Color widgets
        // =======================================================================================================================================
        /**
         * @brief Draws an RGBA color editor.
         * @param label Visible widget label and interaction identifier.
         * @param[in,out] vect Vector or color components edited by the widget.
         * @returns True when a color component changes.
         */
        auto ColorEdit4( eastl::string_view label, core::float4 &vect ) -> bool;

        /**
         * @brief Draws an RGB color editor.
         * @param label Visible widget label and interaction identifier.
         * @param[in,out] vect Vector or color components edited by the widget.
         * @returns True when a color component changes.
         */
        auto ColorEdit3( eastl::string_view label, core::float4 &vect ) -> bool;

        // Invokes ToString() on the enum type to get the string representation of the enum value
        // User needs to provide a ToString() function for the enum type.
        /**
         * @brief Draws a combo box for a set of choices.
         * @tparam EnumType Enumeration whose values select entries in choices.
         * @param choices Selectable labels or values.
         * @param currentSelection Current selection used as the preview.
         * @returns Chosen enumeration value, or currentSelection when no new selection is made.
         */
        template<typename EnumType>
        MKT_NODISCARD auto Combo( eastl::span<EnumType> choices, EnumType currentSelection ) -> EnumType;

        /**
         * @brief Draws a range-backed combo and updates the active selection.
         * @tparam InputIt Iterator over the selectable labels.
         * @tparam Pred Predicate testing whether an item is selected.
         * @param start Iterator to the first selectable item.
         * @param end One-past-last iterator of the borrowed range.
         * @param currentSelection Current selection used as the preview.
         * @param isSelectedPred Predicate identifying the selected item in the range.
         * @param label Visible widget label and interaction identifier.
         */
        template<typename InputIt, typename Pred>
        auto ComboList( InputIt start, InputIt end, eastl::string& currentSelection, Pred &&isSelectedPred, eastl::string_view label ) -> void;

    }// namespace widget

}// namespace mikoto::imgui

#endif// MIKOTO_IMGUI_IMGUI_WIDGET_HH
