/**
 * Panel.hh
 * Created by kate on 6/23/23.
 * */

#ifndef MIKOTO_PANEL_HH
#define MIKOTO_PANEL_HH

// C++ Standard Library
#include <utility>
#include <string_view>

// Project Header
#include <Common/Common.hh>
#include <Library/Random/Random.hh>

namespace Mikoto {
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
        explicit Panel(const std::string_view name = "Panel")
            : m_PanelHeaderName{ name }, m_PanelIsHovered{ false }, m_PanelIsFocused{ false }, m_PanelIsVisible{ true } {
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
        auto operator=( Panel&& other ) -> Panel& = default;


        /**
         * @brief Updates the state of this panel.
         * @param timeStep time elapsed since last frame.
         * */
        virtual auto OnUpdate( float timeStep ) -> void = 0;


        /**
         * @brief Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it will always be visible.
         * */
        auto MakeVisible( const bool value ) -> void { m_PanelIsVisible = value; }


        /**
         * @brief Tells whether this panel is hovered or not.
         * @returns True if this panel is hovered, false otherwise
         * */
        MKT_NODISCARD auto IsHovered() const -> bool { return m_PanelIsHovered; }


        /**
         * @brief Tells whether this panel is focused or not.
         * @returns True if this panel is focused, false otherwise.
         * */
        MKT_NODISCARD auto IsFocused() const -> bool { return m_PanelIsFocused; }


        /**
         * @brief Whether this panel is visible or not.
         * @returns True if this panel is visible, false otherwise.
         * */
        MKT_NODISCARD auto IsVisible() const -> bool { return m_PanelIsVisible; }


        /**
         * Destructor, defaulted
         * */
        virtual ~Panel() = default;


    protected:
        /** Panel title. */
        std::string m_PanelHeaderName{};

        /** Tells whether this panel is hovered (true) or not. */
        bool m_PanelIsHovered{};

        /** Tells whether this panel is focused (true) or note. */
        bool m_PanelIsFocused{};

        /** Tells whether this panel is visible (true) or not. */
        bool m_PanelIsVisible{};
    };
}// namespace Mikoto

#endif // MIKOTO_PANEL_HH