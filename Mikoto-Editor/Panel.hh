/**
 * @file Panel.hh
 * @brief Definition of the Panel class
 * @details Defines an interface for the editor dock-space panels
 * @date 6/23/23.
 * @author kate
 * */

#ifndef MIKOTO_PANEL_HH
#define MIKOTO_PANEL_HH

// C++ Standard Library
#include <utility>

// Project Header
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/Random.hh>

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
        explicit Panel()
            :   m_Guid{}
            ,   m_PanelHeaderName{ "Panel" }
            ,   m_PanelIsHovered{ false }
            ,   m_PanelIsFocused{ false }
            ,   m_PanelIsVisible{ true }
        {

        }


        /**
         * @brief Constructs this panel using move semantics, defaulted.
         * @param other Moved from panel.
         * */
        Panel(Panel&& other) = default;


        /**
         * Assigns other panel to the implicit parameter using move semantics.
         * @param other Moved from panel.
         * @returns *this
         * */
        auto operator=(Panel&& other) -> Panel& = default;


        /**
         * @brief Updates the state of this panel.
         * @param timeStep time elapsed since last frame.
         * */
        virtual auto OnUpdate(float timeStep) -> void = 0;


        /**
         * @brief Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it will always be visible.
         * */
        auto MakeVisible(bool value) -> void { m_PanelIsVisible = value; }


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
         * @brief Returns the universally unique identifier of this panel.
         * @returns this panel's globally unique identifier.
         * */
        MKT_UNUSED_FUNC MKT_NODISCARD auto GetGuid() const -> UInt64_T { return m_Guid.Get(); }


        /**
         * Destructor, defaulted
         * */
        virtual ~Panel() = default;


    protected:
        Random::GUID::UUID m_Guid{};     /**< This panels globally unique identifier. */

        std::string m_PanelHeaderName{}; /**< Panel title. */

        bool m_PanelIsHovered{}; /**< Tells whether this panel is hovered (true) or not. */
        bool m_PanelIsFocused{}; /**< Tells whether this panel is focused (true) or note. */
        bool m_PanelIsVisible{}; /**< Tells whether this panel is visible (true) or not. */
    };
}

#endif // MIKOTO_PANEL_HH