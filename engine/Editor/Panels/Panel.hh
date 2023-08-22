//
// Created by kate on 6/23/23.
//

#ifndef KATE_ENGINE_PANEL_HH
#define KATE_ENGINE_PANEL_HH

#include <utility>

#include <Utility/Common.hh>
#include <Core/Events/Event.hh>

namespace Mikoto {
    /**
     * General interface for panels. Panels are windows that
     * can be dragged around our main window or simply dock
     * into our level editor dock space
     * */
    class Panel {
    public:
        explicit Panel() = default;
        explicit Panel(Path_T  iconPath) : m_IconDirectory{ std::move( iconPath ) } {}
        virtual ~Panel() = default;

        Panel(const Panel& other) = default;
        Panel(Panel&& other) = default;

        auto operator=(const Panel& other) -> Panel& = default;
        auto operator=(Panel&& other) -> Panel& = default;

        MKT_NODISCARD auto GetIconPath() const -> const Path_T& { return m_IconDirectory; }

        virtual auto OnUpdate() -> void = 0;
        virtual auto OnEvent(Event& event) -> void = 0;
        virtual auto MakeVisible(bool value) -> void = 0;

        MKT_NODISCARD virtual auto IsHovered() const -> bool = 0;
        MKT_NODISCARD virtual auto IsFocused() const -> bool = 0;
        MKT_NODISCARD virtual auto IsVisible() const -> bool = 0;
    private:
        Path_T m_IconDirectory{};
    };
}

#endif//KATE_ENGINE_PANEL_HH
