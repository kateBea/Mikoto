//
// Created by kate on 11/23/25.
//

#ifndef MIKOTO_WIN32WINDOW_HH
#define MIKOTO_WIN32WINDOW_HH


#include <Core/Platform.hh>
#include <Platform/Window.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS) && defined(MKT_USE_WIN32_WINDOW)

//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Mikoto {

    
class Win32Window final : public Window {
    public:
        explicit Win32Window( const WindowProperties& props );
        ~Win32Window() override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override { return m_WindowHandle; }

        auto SetScreenMode( ScreenMode mode ) -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool override;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool override;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool override;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool override;

        MKT_NODISCARD auto ShouldClose() const -> bool override;

        MKT_NODISCARD auto GetMouseX() const -> double override;
        MKT_NODISCARD auto GetMouseY() const -> double override;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double> override;

        auto SetShouldClose( const bool value ) -> void { m_ShouldClose = value; }

    private:
        auto RegisterWindowClass() -> void;
        auto CenterWindow() -> void;

    private:
        HWND m_WindowHandle{ nullptr };
        HINSTANCE m_Instance{ nullptr };
        bool m_ShouldClose{ false };

        inline static auto s_ClassName{ "MikotoWin32Window" };
        inline static bool s_ClassRegistered{ false };
        inline static HINSTANCE s_HInstance{ nullptr };

        Int32 m_PrevW{ 0 };
        Int32 m_PrevH{ 0 };
    };

} // namespace Mikoto


#endif

#endif//MIKOTO_WIN32WINDOW_HH
