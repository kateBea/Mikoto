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
#include <Core/Events/Event.hh>

namespace Mikoto {
    /**
     * General interface for panels. Panels are windows that
     * can be dragged around our main window or simply dock
     * into our level editor dock space
     * */

    template<typename PanelType>
    class Panel {
    public:
        /**
         * Defaults constructs this panel
         * */
        explicit Panel() = default;

        /**
         * Constructs this panel with the icon from the given path
         * @param iconPath absolute or relative path to the icon of this panel
         * */
        explicit Panel(Path_T iconPath) : m_IconDirectory{ std::move( iconPath ) } {}

        /**
         * Assigns other panel to the implicit parameter using move semantics, defaulted.
         * @param other moved from panel
         * @returns *this
         * */
        auto operator=(Panel&& other) -> Panel& = default;

        /**
         * Updates the state of this panel
         * */
        auto OnUpdate() -> void { static_cast<PanelType>(this)->OnUpdate(); }

        /**
         * Must be called everytime we want to propagate an event to
         * this panel to be handled
         * @param event event to be handled
         * */
        auto OnEvent(Event& event) -> void { static_cast<PanelType>(this)->OnEvent(event); }

        /**
         * Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it may always be visible
         * */
        auto MakeVisible(bool value) -> void { static_cast<PanelType>(this)->MakeVisible(value); }

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
         * Returns true the absolute path to the icon from this panel
         * @returns path to the icon from this panel
         * */
        MKT_NODISCARD auto GetIconPath() const -> const Path_T& { return m_IconDirectory; }

        /**
         * Destructor, defaulted
         * */
        ~Panel() = default;

    protected:
        /*************************************************************
        * DATA MEMBERS
        * ***********************************************************/
        Path_T m_IconDirectory{};
        bool m_PanelIsHovered{};
        bool m_PanelIsFocused{};
        bool m_PanelIsVisible{};
    };
}

#endif // MIKOTO_PANEL_HH
