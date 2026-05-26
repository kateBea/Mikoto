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

#ifndef MIKOTO_PANEL_HH
#define MIKOTO_PANEL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
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
        explicit Panel(const eastl::string_view name = "Panel", const eastl::string_view headerName = "Panel")
            : mPanelName{ name }, mPanelHeaderName{ headerName }, mPanelIsVisible{ true } {
        }


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
        virtual auto OnUpdate( float timeStep ) -> void = 0;


        /**
         * @brief Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it will always be visible.
         * */
        auto SetVisible( const bool value ) -> void { mPanelIsVisible = value; }

        MKT_NODISCARD auto GetViewport() const -> const ViewportInfo& { return mViewport; }

        /**
         * @brief Tells whether this panel is hovered or not.
         * @returns True if this panel is hovered, false otherwise
         * */
        MKT_NODISCARD auto IsHovered() const -> bool { return mPanelIsHovered; }


        /**
         * @brief Tells whether this panel is focused or not.
         * @returns True if this panel is focused, false otherwise.
         * */
        MKT_NODISCARD auto IsFocused() const -> bool { return mPanelIsFocused; }


        /**
         * @brief Whether this panel is visible or not.
         * @returns True if this panel is visible, false otherwise.
         * */
        MKT_NODISCARD auto IsVisible() const -> bool { return mPanelIsVisible; }

        MKT_NODISCARD auto GetName() const -> eastl::string_view { return mPanelName; }
        MKT_NODISCARD auto GetHeaderName() const -> eastl::string_view { return mPanelHeaderName; }


        /**
         * Destructor, defaulted
         * */
        virtual ~Panel() = default;


    protected:
        eastl::string mPanelName{};
        eastl::string mPanelHeaderName{};

        bool mPanelIsHovered{};
        bool mPanelIsFocused{};
        bool mPanelIsVisible{};

        ViewportInfo mViewport{};
    };
}// namespace Mikoto

#endif // MIKOTO_PANEL_HH