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

#ifndef MIKOTO_PANELS_PANEL_HH
#define MIKOTO_PANELS_PANEL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

namespace mikoto::editor {

    struct ViewportInfo {
        core::f32 mX{};
        core::f32 mY{};
        core::f32 mWidth{};
        core::f32 mHeight{};
    };

    /**
     * General interface for panels. Panels are windows that
     * can be dragged around our main window or simply dock
     * into our level editor dock space
     * */
    class Panel {
    public:
        /**
         * @brief Constructs this panel with the icon from the given path.
         * */
        explicit Panel( eastl::string_view name = "Panel", eastl::string_view headerName = "Panel",
                        eastl::string_view group = "Other", bool isToggleable = true );

        /**
         * @brief Constructs this panel using move semantics, defaulted.
         * @param other Moved from panel.
         * */
        Panel( Panel&& other ) = default;

        /**
         * Assigns other panel to the implicit parameter using move semantics.
         * @param other Moved from panel.
         * @returns *this
         * */
        auto operator=( Panel&& other )  noexcept -> Panel& = default;

        /**
         * @brief Updates the state of this panel.
         * @param timeStep time elapsed since last frame.
         * */
        virtual auto OnRender( float timeStep ) -> void = 0;

        /**
         * @brief Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it will always be visible.
         * */
        auto SetVisible( const bool value ) -> void;

        /**
         * @brief Sets the group used to organize this panel in the Window > Panels menu.
         * @param group Name of the panel group, such as Rendering or Settings.
         * */
        auto SetGroup( eastl::string_view group ) -> void;

        /**
         * @brief Retrieves the group assigned to this panel.
         * @returns A const reference to the panel's group name.
         * */
        MKT_NODISCARD auto GetGroup() const -> const eastl::string&;

        /**
         * @brief Sets whether this panel appears in the Window > Panels menu.
         * @param toggleable True to offer a visibility toggle, false to hide the menu entry.
         * */
        auto SetToggleable( bool toggleable ) -> void;

        /**
         * @brief Tells whether this panel can be toggled from the Window > Panels menu.
         * @returns True if the menu offers a visibility toggle for this panel, false otherwise.
         * */
        MKT_NODISCARD auto IsToggleable() const -> bool;

        /**
        * @brief Retrieves the current viewport information.
        * @return A const reference to the ViewportInfo structure containing  the viewport configuration.
        */
        MKT_NODISCARD auto GetViewport() const -> const ViewportInfo&;

        /**
         * @brief Tells whether this panel is hovered or not.
         * @returns True if this panel is hovered, false otherwise
         * */
        MKT_NODISCARD auto IsHovered() const -> bool;

        /**
         * @brief Tells whether this panel is focused or not.
         * @returns True if this panel is focused, false otherwise.
         * */
        MKT_NODISCARD auto IsFocused() const -> bool;

        /**
         * @brief Whether this panel is visible or not.
         * @returns True if this panel is visible, false otherwise.
         * */
        MKT_NODISCARD auto IsVisible() const -> bool;

        /**
        * @brief Retrieves the unique name identifier of the panel.
        * @return An string view representing the panel's name.
        * */
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        /** @brief Returns the translated panel name without changing its registry identity.
         *
         * @return Display name in the current editor language.
         */
        MKT_NODISCARD auto GetDisplayName() const -> eastl::string_view;

        /**
        * @brief Retrieves the display name used in the panel's header.
        * @return An string view representing the panel's header name.
        * */
        MKT_NODISCARD auto GetHeaderName() const -> eastl::string_view;

        /**
         * Destructor, defaulted
         * */
        virtual ~Panel() = default;

    protected:
        eastl::string mPanelName{};
        eastl::string mPanelHeaderName{};
        eastl::string mGroup{};
        mutable eastl::string mLocalizedHeaderName{};

        bool mPanelIsHovered{};
        bool mPanelIsFocused{};
        bool mPanelIsVisible{};
        bool mIsToggleable{ true };

        ViewportInfo mViewport{};
    };
}// namespace Mikoto

#endif// MIKOTO_PANELS_PANEL_HH

