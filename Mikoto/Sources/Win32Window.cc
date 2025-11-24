/**
 * Win32Window.cc
 * Created by kate on 11/24/25.
 * */

#include <Core/Platform.hh>
#include <Logging/Logger.hh>
#include <Platform/Win32Window.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS) && defined(MKT_USE_WIN32_WINDOW)

namespace Mikoto {

    static LRESULT CALLBACK WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
        switch ( uMsg ) {
            case WM_CLOSE:
                DestroyWindow( hwnd );
                return 0;
            case WM_DESTROY:
                PostQuitMessage( 0 );
                return 0;
            // Handle other messages
        }
        return DefWindowProc( hwnd, uMsg, wParam, lParam );
    }

    Window* Window::Create( const WindowProperties& properties ) {
        return new Win32Window( properties );
    }

    Win32Window::Win32Window( const WindowProperties& props )
        : Window( props ) {
        Init();
    }

    Win32Window::~Win32Window() {
        Shutdown();
    }

    auto Win32Window::ShouldClose() const -> bool {
        return true;
    }

    auto Win32Window::Init() -> void {
        m_Instance = GetModuleHandle( nullptr );

        const char* className = "MikotoWindowClass";

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = m_Instance;
        wc.lpszClassName = className;

        RegisterClass( &wc );

        m_WindowHandle = CreateWindowEx(
            0,
            className,
            m_Properties.Title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, m_Properties.Width, m_Properties.Height,
            nullptr,
            nullptr,
            m_Instance,
            nullptr
        );

        if ( !m_WindowHandle ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create Windows window" );
            return;
        }

        ShowWindow( m_WindowHandle, SW_SHOW );
    }

    auto Win32Window::Shutdown() -> void {
        if ( m_WindowHandle ) {
            DestroyWindow( m_WindowHandle );
            m_WindowHandle = nullptr;
        }
        UnregisterClass( "MikotoWindowClass", m_Instance );
    }

    auto Win32Window::ProcessEvents() -> void {
        MSG msg = {};
        while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    auto Win32Window::GetNativeWindow() const -> std::any {
        return m_WindowHandle;
    }

    auto Win32Window::SetScreenMode( ScreenMode mode ) -> void {
        m_ScreenMode = mode;
        // Implementation for fullscreen switching
    }

    auto Win32Window::IsKeyPressed( KeyCode keyCode ) const -> bool {
        // Map KeyCode to VK_ code
        return false;
    }

    auto Win32Window::IsKeyReleased( KeyCode keyCode ) const -> bool {
        return false;
    }

    auto Win32Window::IsMouseKeyPressed( MouseButton button ) const -> bool {
        return false;
    }

    auto Win32Window::IsMouseKeyReleased( MouseButton button ) const -> bool {
        return false;
    }

    auto Win32Window::GetMouseX() const -> double {
        auto [x, y] = GetMousePos();
        return x;
    }

    auto Win32Window::GetMouseY() const -> double {
        auto [x, y] = GetMousePos();
        return y;
    }

    auto Win32Window::GetMousePos() const -> std::pair<double, double> {
        POINT p;
        if ( GetCursorPos( &p ) && ScreenToClient( m_WindowHandle, &p ) ) {
            return { static_cast<double>( p.x ), static_cast<double>( p.y ) };
        }
        return { 0.0, 0.0 };
    }

} // namespace Mikoto

#endif
