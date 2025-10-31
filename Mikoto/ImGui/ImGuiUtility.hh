/**
 * ImGuiUtility.hh
 * Created by kate on 9/17/23.
 * */

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

#include <any>
// C++ Standard Library
#include <volk.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include <Common/Common.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::ImGuiUtils {

    enum class GuizmoManipulationMode {
        TRANSLATION,
        ROTATION,
        SCALE,
    };

    class ImGuiScopedStyleVar {
    public:
        template<typename... Args>
        explicit ImGuiScopedStyleVar( Args&&... args ) {
            ImGui::PushStyleVar( std::forward<Args>( args )... );
        }

        ~ImGuiScopedStyleVar() {
            ImGui::PopStyleVar();
        }
    };

    class ImGuiScopedBorderColor {
    public:
        explicit ImGuiScopedBorderColor( Vec4F color, float thickness = 1.0f ) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, thickness );
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(color.r, color.g, color.b, color.a));
        }

        ~ImGuiScopedBorderColor() {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    };

    class ImGuiScopedTextFont {
    public:
        static constexpr Int8 Invalid{ -1 };

        explicit ImGuiScopedTextFont( const Int8 index )
            : m_Index{ index }
        {
            if (m_Index != Invalid) {
                ImGui::PushFont( ImGui::GetIO().Fonts->Fonts[index] );
            }
        }

        ~ImGuiScopedTextFont() {
            if (m_Index != Invalid) {
                ImGui::PopFont();
            }
        }

    private:
        Int8 m_Index{};
    };

    MKT_NODISCARD inline auto PushImageButton( UInt64 textureId, ImTextureID textureHandle, const ImVec2 size ) -> bool {
        return ImGui::ImageButton( StringUtils::ToString( textureId ).c_str(), textureHandle, size, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } );
    }

    inline auto ThemeDarkModeAlt() -> void {
        // Setup Dear ImGui style
        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

        // Headers
        style.Colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Buttons
        style.Colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Frame BG
        style.Colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Tabs
        style.Colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
        style.Colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

        // Title
        style.Colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = .5f;
        style.GrabRounding = .5f;
        style.ChildRounding = .5f;
        style.WindowRounding = .5f;
        style.PopupRounding = .5f;
        style.ScrollbarRounding = .5f;
        style.TabRounding = .5f;
    }

    inline auto ThemeDarkModeDefault() -> void {
        // Setup Dear ImGui style
        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_TitleBg] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4( 0.2f, 0.2f, 0.2f, 1.0f );
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );

        style.Colors[ImGuiCol_Tab] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_Button] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_ButtonActive] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );

        style.Colors[ImGuiCol_TabHovered] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_TabActive] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4( 0.20f, 0.20f, 0.20f, 1.0f );

        style.Colors[ImGuiCol_Header] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_HeaderActive] = ImVec4( 0.4f, 0.4f, 0.4f, 1.0f );

        style.Colors[ImGuiCol_MenuBarBg] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_FrameBg] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.2f, 0.2f, 0.2f, 1.0f );
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4( 0.3f, 0.3f, 0.3f, 1.0f );

        style.Colors[ImGuiCol_Border] = ImVec4( 0.26f, 0.26f, 0.26f, 1.0f );
        style.Colors[ImGuiCol_BorderShadow] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_SliderGrab] = ImVec4( 0.10f, 0.10f, 0.10f, 1.0f );
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.1f, 0.1f, 0.1f, 1.0f );
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.16f, 0.16f, 0.16f, 1.0f );

        style.Colors[ImGuiCol_WindowBg] = ImVec4( 0.124f, 0.124f, 0.124f, 1.0f );
        style.Colors[ImGuiCol_ChildBg] = ImVec4( 0.184f, 0.184f, 0.184f, 0.00f );
        style.Colors[ImGuiCol_CheckMark] = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );


        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = .5f;
        style.GrabRounding = .5f;
        style.ChildRounding = .5f;
        style.WindowRounding = .5f;
        style.PopupRounding = .5f;
        style.ScrollbarRounding = .5f;
        style.TabRounding = .5f;
    }

    MKT_NODISCARD inline auto ComputeWidth() -> float {
        const ImGuiContext& globalContext{ *GImGui };
        const ImGuiWindow* currentWindow{ globalContext.CurrentWindow };
        float width{};

        if ( globalContext.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth ) {
            width = globalContext.NextItemData.Width;
        } else {
            width = currentWindow->DC.ItemWidth;
        }

        if ( width < 0.0f ) {
            float regionAvailableX{ ImGui::GetContentRegionAvail().x };
            width = ImMax( 1.0f, regionAvailableX + width );
        }

        width = IM_TRUNC( width );

        return width;
    }

    inline auto HelpMarker( const std::string_view description, const std::string_view placeHolder = "(?)", const bool sameLine = false ) -> void {
        if ( sameLine ) {
            ImGui::SameLine();
        }

        ImGui::TextDisabled( "%s", placeHolder.data() );
        if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort ) && ImGui::BeginTooltip() ) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
            ImGui::TextUnformatted( description.data() );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    inline auto CheckBox( const CStr label, bool& value ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };

        bool active{ ImGui::Checkbox( label, std::addressof( value ) ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto ToolTip( const std::string_view description ) -> void {
        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if ( ImGui::BeginTooltip() ) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
            ImGui::TextUnformatted( description.data() );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    inline auto ToolTip( const std::function<void()>& func, const bool enable ) -> void {
        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if ( enable && ImGui::BeginTooltip() ) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );

            func();

            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    inline auto DragFloat4( const CStr label, const CStr format, glm::vec4& vect, float speed, float minVal, float maxVal ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        bool active{ ImGui::DragFloat4( label, value_ptr( vect ), speed, minVal, maxVal, format ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto ColorEdit4( const CStr label, glm::vec4& vect ) -> bool {
        constexpr ImGuiColorEditFlags colorEditFlags{
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar
        };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 5.0f, 5.0f } };

        const bool active{ ImGui::ColorEdit4( label, value_ptr( vect ), colorEditFlags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto Slider( const CStr label, float& value, const glm::vec2& bounds, std::string_view format = "%.2f" ) -> bool {
        constexpr ImGuiSliderFlags flags{ ImGuiSliderFlags_None };

        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        const bool active{ ImGui::SliderFloat( label, std::addressof( value ), bounds.x, bounds.y, format.data(), flags ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto ButtonTextIcon( const CStr icon, ImVec2 size = { 0.0f, 0.0f } ) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 0.0f, 0.0f } };
        ImGuiScopedStyleVar framePadding{ ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } };

        const bool active{ ImGui::Button( fmt::format( "{}", icon ).c_str(), size ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        return active;
    }

    inline auto TextArea( std::string& buffer) -> bool {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };

        const auto resizeCallback{ []( ImGuiInputTextCallbackData* data ) -> Int32 {
            if ( data->EventFlag == ImGuiInputTextFlags_CallbackResize ) {
                std::string* str{ static_cast<std::string*>( data->UserData ) };

                str->resize( data->BufTextLen );
                data->Buf = str->data();
            }
            return 0;
        } };

        constexpr ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize };

        const ImVec2 windowSize{ ImGui::GetWindowSize() };

        constexpr float maxScale{ 3 };
        constexpr float minScale{ 1 };

        const bool active{ ImGui::InputTextMultiline( "##TextArea:Input", buffer.data(), buffer.capacity() + 1,
                                                      ImVec2( ComputeWidth(), windowSize.y * 0.3f ), flags, resizeCallback, std::addressof( buffer ) ) };

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_TextInput );
        }

        ImGui::SetWindowFontScale( minScale );

        return active;
    }

    template<typename InputIt, typename Pred>
    inline auto ComboList( InputIt start, InputIt end, std::string& currentlyActive, Pred&& isSelectedPred, const CStr label ) -> void {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar popUpRounding{ ImGuiStyleVar_PopupRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f, 10.0f } };

        constexpr ImGuiComboFlags comboFlags{ ImGuiComboFlags_None };
        constexpr ImGuiSelectableFlags selectableFlags{ ImGuiSelectableFlags_None };

        if ( ImGui::BeginCombo( fmt::format( "##{}", label ).c_str(), currentlyActive.c_str(), comboFlags ) ) {

            for ( ; start != end; ++start ) {
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

    inline auto CenteredText( const char* label, const float width, float height = 20.0f ) -> void {
        // https://github.com/phicore/ImGuiStylingTricks/wiki/Custom-MessageBox#step-5-removed-title-bar-and-homemade-centered-text

        ImGuiContext& g{ *GImGui };
        const ImGuiStyle& style{ g.Style };

        const ImVec2 textSize{ width, height };
        const ImGuiWindow* window{ ImGui::GetCurrentWindow() };

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

    inline auto DragDropDemo() -> void {
        if ( ImGui::TreeNode( "Drag and Drop" ) ) {
            if ( ImGui::TreeNode( "Drag and drop in standard widgets" ) ) {
                // ColorEdit widgets automatically act as drag source and drag target.
                // They are using standardized payload strings IMGUI_PAYLOAD_TYPE_COLOR_3F and IMGUI_PAYLOAD_TYPE_COLOR_4F
                // to allow your own widgets to use colors in their drag and drop interaction.
                // Also see 'Demo->Widgets->Color/Picker Widgets->Palette' demo.
                HelpMarker( "You can drag from the color squares." );
                static float col1[3] = { 1.0f, 0.0f, 0.2f };
                static float col2[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
                ImGui::ColorEdit3( "color 1", col1 );
                ImGui::ColorEdit4( "color 2", col2 );
                ImGui::TreePop();
            }

            if ( ImGui::TreeNode( "Drag and drop to copy/swap items" ) ) {
                enum Mode {
                    Mode_Copy,
                    Mode_Move,
                    Mode_Swap
                };
                static int mode = 0;
                if ( ImGui::RadioButton( "Copy", mode == Mode_Copy ) ) { mode = Mode_Copy; }
                ImGui::SameLine();
                if ( ImGui::RadioButton( "Move", mode == Mode_Move ) ) { mode = Mode_Move; }
                ImGui::SameLine();
                if ( ImGui::RadioButton( "Swap", mode == Mode_Swap ) ) { mode = Mode_Swap; }
                static const char* names[9] = {
                    "Bobby", "Beatrice", "Betty",
                    "Brianna", "Barry", "Bernard",
                    "Bibi", "Blaine", "Bryn"
                };
                for ( int n = 0; n < IM_ARRAYSIZE( names ); n++ ) {
                    ImGui::PushID( n );
                    if ( ( n % 3 ) != 0 )
                        ImGui::SameLine();
                    ImGui::Button( names[n], ImVec2( 60, 60 ) );

                    // Our buttons are both drag sources and drag targets here!
                    if ( ImGui::BeginDragDropSource( ImGuiDragDropFlags_None ) ) {
                        // Set payload to carry the index of our item (could be anything)
                        ImGui::SetDragDropPayload( "DND_DEMO_CELL", &n, sizeof( int ) );

                        // Display preview (could be anything, e.g. when dragging an image we could decide to display
                        // the filename and a small preview of the image, etc.)
                        if ( mode == Mode_Copy ) { ImGui::Text( "Copy %s", names[n] ); }
                        if ( mode == Mode_Move ) { ImGui::Text( "Move %s", names[n] ); }
                        if ( mode == Mode_Swap ) { ImGui::Text( "Swap %s", names[n] ); }
                        ImGui::EndDragDropSource();
                    }
                    if ( ImGui::BeginDragDropTarget() ) {
                        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "DND_DEMO_CELL" ) ) {
                            IM_ASSERT( payload->DataSize == sizeof( int ) );
                            int payload_n = *( const int* )payload->Data;
                            if ( mode == Mode_Copy ) {
                                names[n] = names[payload_n];
                            }
                            if ( mode == Mode_Move ) {
                                names[n] = names[payload_n];
                                names[payload_n] = "";
                            }
                            if ( mode == Mode_Swap ) {
                                const char* tmp = names[n];
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

            if ( ImGui::TreeNode( "Drag to reorder items (simple)" ) ) {
                // FIXME: there is temporary (usually single-frame) ID Conflict during reordering as a same item may be submitting twice.
                // This code was always slightly faulty but in a way which was not easily noticeable.
                // Until we fix this, enable ImGuiItemFlags_AllowDuplicateId to disable detecting the issue.
                ImGui::PushItemFlag( ImGuiItemFlags_AllowDuplicateId, true );

                // Simple reordering
                HelpMarker(
                        "We don't use the drag and drop api at all here! "
                        "Instead we query when the item is held but not hovered, and order items accordingly." );
                static const char* item_names[] = { "Item One", "Item Two", "Item Three", "Item Four", "Item Five" };
                for ( int n = 0; n < IM_ARRAYSIZE( item_names ); n++ ) {
                    const char* item = item_names[n];
                    ImGui::Selectable( item );

                    if ( ImGui::IsItemActive() && !ImGui::IsItemHovered() ) {
                        int n_next = n + ( ImGui::GetMouseDragDelta( 0 ).y < 0.f ? -1 : 1 );
                        if ( n_next >= 0 && n_next < IM_ARRAYSIZE( item_names ) ) {
                            item_names[n] = item_names[n_next];
                            item_names[n_next] = item;
                            ImGui::ResetMouseDragDelta();
                        }
                    }
                }

                ImGui::PopItemFlag();
                ImGui::TreePop();
            }

            if ( ImGui::TreeNode( "Tooltip at target location" ) ) {
                for ( int n = 0; n < 2; n++ ) {
                    // Drop targets
                    ImGui::Button( n ? "drop here##1" : "drop here##0" );
                    if ( ImGui::BeginDragDropTarget() ) {
                        ImGuiDragDropFlags drop_target_flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoPreviewTooltip;
                        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( IMGUI_PAYLOAD_TYPE_COLOR_4F, drop_target_flags ) ) {
                            IM_UNUSED( payload );
                            ImGui::SetMouseCursor( ImGuiMouseCursor_NotAllowed );
                            ImGui::SetTooltip( "Cannot drop here!" );
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Drop source
                    static ImVec4 col4 = { 1.0f, 0.0f, 0.2f, 1.0f };
                    if ( n == 0 )
                        ImGui::ColorButton( "drag me", col4 );
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }

    /**
     * Utility function to make panel names for ImGui windows.
     * @param panelIcon Panel's icon value.
     * @param panelName Name of the panel.
     * @returns The panel's name including the icon.
     * */
    MKT_NODISCARD inline auto MakePanelName(std::string_view panelIcon, std::string_view panelName) -> std::string {
        return fmt::format("{} {}", panelIcon, panelName);
    }
}// namespace Mikoto::ImGuiUtils

#endif// MIKOTO_IMGUI_UTILS_HH
