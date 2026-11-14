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

#ifndef MIKOTO_IMGUI_UTILITY_HH
#define MIKOTO_IMGUI_UTILITY_HH

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/utility.h>
#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <glm/gtc/type_ptr.hpp>

#include <imgui_node_editor.h>

#include <magic_enum/magic_enum.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>

namespace mikoto::imgui {

    // Window titles and docking tabs retain their larger padding independently of controls.
    inline const ImVec2 kWindowTabPadding{ 10.0f, 6.0f };

    /**
     * @brief Begins a window with the larger title/tab sizing, then restores control sizing.
     * @details Pair every call with ImGui::End(), including when this returns false.
     */
    auto BeginWindow( const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0 ) -> bool;

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
        /**
         * @brief Removes indentation until this scope is destroyed.
         * @param width Indentation in pixels; zero uses ImGui's default spacing.
         */
        explicit UnindentScoped( core::u32 width = 0 );

        /** @brief Restores the indentation removed by this scope. */
        ~UnindentScoped();

    private:
        core::u32 mWidth{};
    };

    class ImGuiScopedStyleVar {
    public:
        /**
         * @brief Pushes an ImGui style variable for this scope's lifetime.
         * @tparam Args Argument types accepted by ImGui::PushStyleVar.
         * @param args Style-variable index and its scalar or vector value.
         */
        template<typename... Args>
        explicit ImGuiScopedStyleVar( Args &&... args ) {
            MKT_BEGIN_PROFILER();
            ImGui::PushStyleVar( eastl::forward<Args>( args )... );
        }

        /** @brief Pops the style variable owned by this scope. */
        ~ImGuiScopedStyleVar();
    };

    class ImGuiScopedColor {
    public:
        /**
         * @brief Pushes an ImGui theme color for this scope's lifetime.
         * @tparam Args Argument types accepted by ImGui::PushStyleColor.
         * @param args Color index and its packed or vector color value.
         */
        template<typename... Args>
        explicit ImGuiScopedColor( Args&&... args ) {
            MKT_BEGIN_PROFILER();
            ImGui::PushStyleColor(  eastl::forward<Args>( args )... );
        }

        /** @brief Pops the color owned by this scope. */
        ~ImGuiScopedColor();
    };

    class ImGuiScopedBorderColor {
    public:
        /**
         * @brief Applies a window border color and thickness for this scope.
         * @param color Border RGBA color.
         * @param thickness Border thickness in pixels.
         */
        explicit ImGuiScopedBorderColor( core::float4 color, float thickness = 1.0f );

        /** @brief Restores the previous border color and thickness. */
        ~ImGuiScopedBorderColor();
    };

    class ImGuiScopedTextFont {
    public:
        static constexpr core::i8 Invalid{ -1 };

        /** @brief Pushes the atlas font at index, or leaves the stack unchanged for Invalid. */
        explicit ImGuiScopedTextFont( core::i8 index );

        /**
         * @brief Pushes a font for the lifetime of this scope.
         * @param font Borrowed atlas font, or null to leave the stack unchanged.
         */
        explicit ImGuiScopedTextFont( ImFont* font );

        /** @brief Pops the font only when this scope owns a stack entry. */
        ~ImGuiScopedTextFont();

        // A font stack entry has one owner. Copies would pop it twice;
        // moving transfers ownership and leaves the source inactive.
        ImGuiScopedTextFont( const ImGuiScopedTextFont& ) = delete;
        auto operator=( const ImGuiScopedTextFont& ) -> ImGuiScopedTextFont& = delete;

        /**
         * @brief Transfers ownership of a font-stack entry.
         * @param[in,out] other Source scope, left inactive after the transfer.
         */
        ImGuiScopedTextFont( ImGuiScopedTextFont&& other ) noexcept;
        auto operator=( ImGuiScopedTextFont&& ) -> ImGuiScopedTextFont& = delete;

    private:
        ImFont* mFont{};
    };

    /**
     * @brief Measures the current window's available content width.
     * @returns Usable width in pixels.
     */
    auto ComputeWidth() -> float;

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
     * @brief Draws text centered within the supplied rectangle.
     * @param label Visible text; literal ## sequences are preserved.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param wrap Whether long lines wrap within the rectangle's horizontal padding.
     */
    auto CenteredText( const char *label, float width, float height = 20.0f, bool wrap = false ) -> void;

    /**
     * @brief Draws the drag-and-drop demonstration controls.
     */
    auto DragDropDemo() -> void;

    /**
     * @brief Encodes a Unicode code point as UTF-8.
     * @param codePoint Unicode code point to encode.
     * @returns UTF-8 byte sequence.
     */
    MKT_NODISCARD auto GetStringFromUnicode( core::u32 codePoint ) -> eastl::string;

    /**
     * @brief Displays material icon glyphs for inspection.
     */
    auto DebugShowMaterialIcons() -> void;

    /**
     * @brief Displays a hexadecimal view of borrowed bytes.
     * @param memory Readable memory to display.
     * @param size Number of readable bytes to display.
     * @param baseAddress Address displayed for the first byte.
     * @param bytesPerRow Number of bytes displayed on each row.
     */
    auto DrawMemoryVisualizer( const void *memory, core::usize size, std::uintptr_t baseAddress, core::usize bytesPerRow = 16 ) -> void;

    /**
     * @brief Selects the hand cursor while the last item is hovered.
     */
    auto SetCursorHandOnLastItemHovered() -> void;

    enum class BlueprintPinShape {
        eData,
        eFlow,
        eObject
    };

    /** @brief Applies the shared Blueprint theme to the current node editor; callers restore its previous style. */
    auto ApplyBlueprintStyle() -> void;

    /**
     * @brief Draws a rounded gradient header after EndNode has resolved the node width.
     * @param node Node whose header is being drawn.
     * @param start Top-left corner of the header in screen pixels.
     * @param color Color used to draw the element.
     */
    auto DrawBlueprintHeader( ax::NodeEditor::NodeId node, const ImVec2& start, const ImVec4& color ) -> void;

    /**
     * @brief Uses imgui-node-editor's Blueprint example icons, filled when connected. Emits an ImGui item.
     * @param shape Blueprint pin outline shape.
     * @param connected Whether the pin is connected.
     * @param color Color used to draw the element.
     */
    auto DrawBlueprintPinIcon( BlueprintPinShape shape, bool connected, const ImVec4& color ) -> void;

}// namespace mikoto::imgui

#endif// MIKOTO_IMGUI_UTILITY_HH
