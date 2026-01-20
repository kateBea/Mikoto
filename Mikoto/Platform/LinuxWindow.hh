/**
* LinuxWindow.hh
 * Created by kate on 11/24/25.
 * */

#ifndef MIKOTO_PLATFORM_LINUX_LINUXWINDOW_HH
#define MIKOTO_PLATFORM_LINUX_LINUXWINDOW_HH

#include <Core/Platform.hh>

#include <Platform/Window.hh>

#if defined(MIKOTO_PLATFORM_LINUX) && defined(MKT_USE_XCB_WINDOW)
#include <xcb/xcb.h>

namespace Mikoto {

    class LinuxWindow final : public Window {
    public:
        explicit LinuxWindow( const WindowProperties& props );
        ~LinuxWindow() override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override;

        auto SetScreenMode( ScreenMode mode ) -> void override;

        auto SetCursorMode( CursorMode mode ) -> void override;
        auto SetCursorType( CursorType type ) -> void override;

        auto ResetCursorType() -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool override;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool override;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool override;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool override;

        MKT_NODISCARD auto GetMouseX() const -> double override;
        MKT_NODISCARD auto GetMouseY() const -> double override;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double> override;

        MKT_NODISCARD auto ShouldClose() const -> bool override;

    private:
        xcb_connection_t* m_Connection{ nullptr };
        xcb_window_t m_Window{ 0 };
        xcb_screen_t* m_Screen{ nullptr };
        xcb_intern_atom_reply_t* m_AtomWmDeleteWindow{ nullptr };

        bool m_ShouldClose{ false };
    };

} // namespace Mikoto

#endif

#endif // MIKOTO_PLATFORM_LINUX_LINUXWINDOW_HH
