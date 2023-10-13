/**
 * Panel.hh
 * Created by kate on 6/23/23.
 * */

#ifndef MIKOTO_PANEL_HH
#define MIKOTO_PANEL_HH

// C++ Standard Library
#include <utility>

// Project Header
#include <Utility/Common.hh>
#include <Utility/Random.hh>
#include <Core/Event.hh>

namespace Mikoto {
    /**
     * General interface for panels. Panels are windows that
     * can be dragged around our main window or simply dock
     * into our level editor dock space
     * */
    class Panel {
    public:
        /**
         * Constructs this panel with the icon from the given path
         * @param iconPath absolute or relative path to the icon of this panel
         * */
        explicit Panel()
            :   m_PanelIsHovered{ false }
            ,   m_PanelIsFocused{ false }
            ,   m_PanelIsVisible{ true }
            ,   m_Guid{ }
        {

        }

        /**
         * Constructs this panel using move semantics, defaulted.
         * @param other moved from panel
         * */
        Panel(Panel&& other) = default;

        /**
         * Assigns other panel to the implicit parameter using move semantics, defaulted.
         * @param other moved from panel
         * @returns *this
         * */
        auto operator=(Panel&& other) -> Panel& = default;

        /**
         * Updates the state of this panel.
         * */
        virtual auto OnUpdate(float timeStep) -> void = 0;


        /**
         * Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it may always be visible
         * */
        auto MakeVisible(bool value) -> void { m_PanelIsVisible = value; }

        /**
         * Returns true if this panel is hovered, false otherwise
         * @returns whether this panel is hovered or not
         * */
        MKT_NODISCARD auto IsHovered() const -> bool { return m_PanelIsHovered; }

        /**
         * Returns true if this panel is focused, false otherwise
         * @returns whether this panel is focused or not
         * */
        MKT_NODISCARD auto IsFocused() const -> bool { return m_PanelIsFocused; }

        /**
         * Returns true if this panel is visible, false otherwise
         * @returns whether this panel is visible or not
         * */
        MKT_NODISCARD auto IsVisible() const -> bool { return m_PanelIsVisible; }

        /**
         * Returns the universally unique identifier of this panel
         * @returns this panel's GUID
         * */
        MKT_UNUSED_FUNC MKT_NODISCARD auto GetGuid() const -> UInt64_T { return m_Guid.Get(); }

        /**
         * Destructor, defaulted
         * */
        virtual ~Panel() = default;

    protected:
        bool m_PanelIsHovered{};
        bool m_PanelIsFocused{};
        bool m_PanelIsVisible{};

        std::string m_PanelHeaderName{};

        // Uniquely identifies this panel
        // Cna be used to subscribe this panel to events
        Random::GUID::UUID m_Guid{};
    };
}

#endif // MIKOTO_PANEL_HH
