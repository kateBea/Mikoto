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

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

#include <imgui.h>
#include <imgui_internal.h>

#include <Core/Core.hh>
#include <Core/Reflect.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Logging/Assert.hh>
#include <glm/gtc/type_ptr.hpp>

namespace mikoto::imgui {

    // TODO: this file will include ImGui helpers, for instance color converters
    // string formatters, etc, widgets are moved to widget namespace

    using namespace mikoto::core;

    enum class GizmoType {
        eTranslation,
        eRotation,
        eScale,
        eBounds,
    };

    enum class GizmoMode {
        eLocal,
        eWorld
    };

    class UnindentScoped {
    public:
        explicit UnindentScoped(u32 width = 0)
            : m_Width{ width } { ImGui::Unindent( m_Width ); }

        ~UnindentScoped() { ImGui::Indent( m_Width ); }
    private:
        u32 m_Width{};
    };

    class ImGuiScopedStyleVar {
    public:
        template<typename... Args>
        explicit ImGuiScopedStyleVar( Args &&... args ) {
            ImGui::PushStyleVar( eastl::forward<Args>( args )... );
        }

        ~ImGuiScopedStyleVar() { ImGui::PopStyleVar(); }
    };

    class ImGuiScopedColor {
    public:
        template<typename... Args>
        explicit ImGuiScopedColor( Args&&... args ) {
            ImGui::PushStyleColor(  eastl::forward<Args>( args )... );
        }

        ~ImGuiScopedColor() {
            ImGui::PopStyleColor();
        }
    };

    class ImGuiScopedBorderColor {
    public:
        explicit ImGuiScopedBorderColor( float4 color, float thickness = 1.0f ) {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, thickness );
            ImGui::PushStyleColor( ImGuiCol_Border, IM_COL32( color.r, color.g, color.b, color.a ) );
        }

        ~ImGuiScopedBorderColor() {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    };

    class ImGuiScopedTextFont {
    public:
        static constexpr i8 Invalid{ -1 };

        explicit ImGuiScopedTextFont( const i8 index )
            : m_Index{ index } { if (m_Index != Invalid) { ImGui::PushFont( ImGui::GetIO().Fonts->Fonts[index] ); } }

        ~ImGuiScopedTextFont() { if (m_Index != Invalid) { ImGui::PopFont(); } }

    private:
        i8 m_Index{};
    };

    auto PushImageButton( u64 textureId, ImTextureID textureHandle, ImVec2 size ) -> bool;
    auto PushImageButton( eastl::string_view ID, ImTextureID textureHandle, ImVec2 size ) -> bool;

    auto ComputeWidth() -> float;

    auto CheckBox( eastl::string_view label, bool &value ) -> bool;

    auto ToolTip( eastl::string_view description ) -> void;

    auto ToolTip( const eastl::function<void()> &func, bool enable ) -> void;

    auto DragFloat4( eastl::string_view label, eastl::string_view format, glm::vec4 &vect, float speed, float minVal, float maxVal ) -> bool;

    auto DragFloat3( eastl::string_view label, eastl::string_view format, glm::vec3 &vect, float speed, float minVal, float maxVal ) -> bool;

    auto ColorEdit4( eastl::string_view label, glm::vec4 &vect ) -> bool;

    auto ColorEdit3( eastl::string_view label, glm::vec3 &vect ) -> bool;

    auto Slider( eastl::string_view label, float &value, const glm::vec2 &bounds, eastl::string_view format = "%.2f" ) -> bool;

    auto ButtonTextIcon( eastl::string_view icon, ImVec2 size = { 0.0f, 0.0f } ) -> bool;

    auto TextArea( eastl::string &buffer ) -> bool;

    auto CenteredText( const char *label, float width, float height = 20.0f ) -> void;

    auto DragDropDemo() -> void;

    MKT_NODISCARD auto GetStringFromUnicode( u32 codePoint ) -> eastl::string;

    auto DebugShowMaterialIcons() -> void;

    auto DrawMemoryVisualizer( const void *memory, size_t size, std::uintptr_t baseAddress, size_t bytesPerRow = 16 ) -> void;

    auto SetCursorHandOnLastItemHovered() -> void;

    MKT_NODISCARD auto Combo(eastl::string* choices, size_t count, const eastl::string& currentSelection) -> i32;

    auto InputText(eastl::string_view viewData, bool readOnly = false) -> bool;

    auto InputText(eastl::string& viewData, ImGuiTextFlags flags ) -> bool;

    template<typename InputIt, typename Pred>
    auto ComboList( InputIt start, InputIt end, eastl::string &currentlyActive, Pred &&isSelectedPred, const eastl::string_view label ) -> void {
        ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.2f };
        ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 2.5f };
        ImGuiScopedStyleVar popUpRounding{ ImGuiStyleVar_PopupRounding, 2.5f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f, 10.0f } };

        constexpr ImGuiComboFlags comboFlags{ ImGuiComboFlags_None };
        constexpr ImGuiSelectableFlags selectableFlags{ ImGuiSelectableFlags_None };

        if (ImGui::BeginCombo( fmt::format( "##{}", label ).c_str(), currentlyActive.c_str(), comboFlags )) {
            for (; start != end; ++start) {
                const bool isSelected{ isSelectedPred( *start ) };

                if (ImGui::Selectable( fmt::format( " {}", *start ).c_str(), isSelected, selectableFlags )) { currentlyActive = *start; }

                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

                if (isSelected) { ImGui::SetItemDefaultFocus(); }
            }

            ImGui::EndCombo();
        }

        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }
    }

    template<typename UIFunction, typename... Args>
    auto DrawNode( const std::string_view label, const UIFunction& uiFunc, Args&&... args ) -> void {
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } );

        const std::string panelName{ fmt::format( "{}{}", __PRETTY_FUNCTION__, label ) };
        const bool componentNodeOpen{
            ImGui::TreeNodeEx( reinterpret_cast<const void*>( label.data() ), treeNodeFlags, "%s", label.data() )
        };

        // Node frame is hovered
        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::PopStyleVar();

        if ( componentNodeOpen ) {

            uiFunc( std::forward<Args>( args )... );

            ImGui::TreePop();
        }
    }

    // TODO: Use enum reflection for this
    template<typename EnumType>
    MKT_NODISCARD auto Combo(std::span<std::string> choices, EnumType currentSelection) -> EnumType {
        MKT_ASSERT( static_cast<u32>( currentSelection ) < choices.size(), "Enum value must be lower than choices size" );

        EnumType result{ currentSelection };

        const std::string &currentChoiceStr{ choices[static_cast<u32>( currentSelection )] };
        const std::string labelName{ fmt::format( "##{}{}", __PRETTY_FUNCTION__, currentChoiceStr ) };

        if ( ImGui::BeginCombo( labelName.data(), currentChoiceStr.c_str() ) ) {
            u32 selectionIndex{};

            for ( const std::string &selectionStr: choices ) {
                const bool isSelected{ selectionStr == choices[static_cast<u32>( currentSelection )] };

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
}

#endif// MIKOTO_IMGUI_UTILS_HH
