/**
 * Win32Window.cc
 * Created by kate on 11/24/25.
 * */

#include <Core/CoreEvents.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Platform/Win32Window.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS ) && defined( MKT_USE_WIN32_WINDOW )

#include <windows.h>
#include <windowsx.h>

namespace Mikoto {

    static constexpr const char* s_ClassName = "MikotoWin32Window";
    static bool s_ClassRegistered = false;

    // Forward declaration
    LRESULT CALLBACK Win32WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

    Win32Window::Win32Window( const WindowProperties& props )
        : Window( props ) {
        m_PrevW = props.Width;
        m_PrevH = props.Height;
    }

    Win32Window::~Win32Window() {
        Shutdown();
    }

    auto Win32Window::RegisterWindowClass() -> void {
        if ( s_ClassRegistered ) {
            return;
        }

        WNDCLASSEX wc{};
        wc.cbSize = sizeof( wc );
        wc.lpfnWndProc = Win32WndProc;
        wc.hInstance = m_Instance;
        wc.lpszClassName = s_ClassName;
        wc.hCursor = LoadCursor( nullptr, IDC_ARROW );
        wc.style = CS_HREDRAW | CS_VREDRAW;

        RegisterClassEx( &wc );
        s_ClassRegistered = true;

        MKT_CORE_LOGGER_INFO( "Cursor Handle: {}", ( void* )wc.hCursor );
        MKT_CORE_LOGGER_INFO( "Registered Win32 Window Class: {}", s_ClassName );
        MKT_CORE_LOGGER_INFO( "Initialized Win32 Window Class successfully. Dimensions {}, {}", m_Properties.Width, m_Properties.Height );
    }

    auto Win32Window::Init() -> void {
        m_Instance = GetModuleHandle( nullptr );
        RegisterWindowClass();

        DWORD style{ WS_OVERLAPPEDWINDOW };
        DWORD exStyle{ WS_EX_APPWINDOW };

        if (!m_Properties.Resizable) {
            style &= ~WS_THICKFRAME;
        }

        RECT rect{ 0, 0, m_Properties.Width, m_Properties.Height };
        AdjustWindowRectEx( &rect, style, FALSE, exStyle );

        m_WindowHandle = CreateWindowEx(
                exStyle,
                s_ClassName,
                m_Properties.Title.c_str(),
                style,
                CW_USEDEFAULT, CW_USEDEFAULT,
                rect.right - rect.left,
                rect.bottom - rect.top,
                nullptr,
                nullptr,
                m_Instance,
                this );

        MKT_ASSERT( m_WindowHandle != nullptr, "Win32Window creation failed!" );

        CenterWindow();
        ShowWindow( m_WindowHandle, SW_SHOW );
        UpdateWindow( m_WindowHandle );
    }

    auto Win32Window::Shutdown() -> void {
        if ( m_WindowHandle ) {
            DestroyWindow( m_WindowHandle );
            m_WindowHandle = nullptr;
        }
    }

    auto Win32Window::ProcessEvents() -> void {
        MSG msg{};
        while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    auto Win32Window::ShouldClose() const -> bool {
        return m_ShouldClose;
    }

    auto Win32Window::IsKeyPressed( KeyCode key ) const -> bool {
        return ( GetAsyncKeyState( key ) & 0x8000 ) != 0;
    }

    auto Win32Window::IsKeyReleased( KeyCode key ) const -> bool {
        return ( GetAsyncKeyState( key ) & 0x8000 ) == 0;
    }

    auto Win32Window::IsMouseKeyPressed( MouseButton b ) const -> bool {
        return ( GetAsyncKeyState( b ) & 0x8000 ) != 0;
    }

    auto Win32Window::IsMouseKeyReleased( MouseButton b ) const -> bool {
        return ( GetAsyncKeyState( b ) & 0x8000 ) == 0;
    }

    auto Win32Window::GetMousePos() const -> std::pair<double, double> {
        POINT p{};
        GetCursorPos( &p );
        ScreenToClient( m_WindowHandle, &p );
        return { ( double )p.x, ( double )p.y };
    }

    auto Win32Window::GetMouseX() const -> double {
        return GetMousePos().first;
    }

    auto Win32Window::GetMouseY() const -> double {
        return GetMousePos().second;
    }

    auto Win32Window::CenterWindow() -> void {
        RECT rc{};
        GetWindowRect( m_WindowHandle, &rc );

        Int32 winW{ rc.right - rc.left };
        Int32 winH{ rc.bottom - rc.top };

        Int32 screenW{ GetSystemMetrics( SM_CXSCREEN ) };
        Int32 screenH{ GetSystemMetrics( SM_CYSCREEN ) };

        Int32 x{ ( screenW - winW ) / 2 };
        Int32 y{ ( screenH - winH ) / 2 };

        SetWindowPos( m_WindowHandle, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
    }

    auto Win32Window::SetScreenMode( ScreenMode mode ) -> void {
        m_ScreenMode = mode;

        switch ( mode ) {
            case ScreenMode::WINDOW_MODE_WINDOWED: {
                SetWindowLongPtr( m_WindowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW );
                SetWindowPos( m_WindowHandle, nullptr,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              m_PrevW, m_PrevH,
                              SWP_FRAMECHANGED | SWP_SHOWWINDOW );
                break;
            }

            case ScreenMode::WINDOW_MODE_FULLSCREEN: {
                MONITORINFO mi{ sizeof( mi ) };
                HMONITOR mon = MonitorFromWindow( m_WindowHandle, MONITOR_DEFAULTTOPRIMARY );
                GetMonitorInfo( mon, &mi );

                m_PrevW = m_Properties.Width;
                m_PrevH = m_Properties.Height;

                SetWindowLongPtr( m_WindowHandle, GWL_STYLE, WS_POPUP );
                SetWindowPos( m_WindowHandle, HWND_TOP,
                              mi.rcMonitor.left, mi.rcMonitor.top,
                              mi.rcMonitor.right - mi.rcMonitor.left,
                              mi.rcMonitor.bottom - mi.rcMonitor.top,
                              SWP_FRAMECHANGED | SWP_SHOWWINDOW );
                break;
            }

            case ScreenMode::WINDOW_MODE_BORDERLESS: {
                MONITORINFO mi{ sizeof( mi ) };
                HMONITOR mon = MonitorFromWindow( m_WindowHandle, MONITOR_DEFAULTTOPRIMARY );
                GetMonitorInfo( mon, &mi );

                m_PrevW = m_Properties.Width;
                m_PrevH = m_Properties.Height;

                SetWindowLongPtr( m_WindowHandle, GWL_STYLE, WS_POPUP );
                SetWindowPos( m_WindowHandle, HWND_TOP,
                              mi.rcWork.left, mi.rcWork.top,
                              mi.rcWork.right - mi.rcWork.left,
                              mi.rcWork.bottom - mi.rcWork.top,
                              SWP_FRAMECHANGED | SWP_SHOWWINDOW );
                break;
            }
        }
    }

    LRESULT CALLBACK Win32WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
        Win32Window* window = reinterpret_cast<Win32Window*>(
                GetWindowLongPtr( hwnd, GWLP_USERDATA ) );

        if ( msg == WM_NCCREATE ) {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>( lParam );
            window = reinterpret_cast<Win32Window*>( cs->lpCreateParams );
            SetWindowLongPtr( hwnd, GWLP_USERDATA, ( LONG_PTR )window );
        }

        if ( !window )
            return DefWindowProc( hwnd, msg, wParam, lParam );

        switch ( msg ) {
            case WM_CLOSE:
                window->SetShouldClose( true );
                //EventService::Get()->Queue<WindowCloseEvent>();
                return 0;

            case WM_SIZE: {
                window->SetWidth( LOWORD( lParam ) );
                window->SetHeight( HIWORD( lParam ) );
                return 0;
            }

            case WM_PAINT:
                ValidateRect( hwnd, nullptr );
                return 0;

            case WM_KEYDOWN:
                MKT_CORE_LOGGER_INFO( "Key Down: {}", ( int )wParam );
                //EventService::Get()->Queue<KeyPressedEvent>( ( KeyCode )wParam, false, 0 );
                return 0;

            case WM_KEYUP:
                MKT_CORE_LOGGER_INFO( "Key Up: {}", ( int )wParam );
                //EventService::Get()->Queue<KeyReleasedEvent>( ( KeyCode )wParam );
                return 0;

            case WM_CHAR:
                MKT_CORE_LOGGER_INFO( "Key Char: {}", ( char )wParam );
                //EventService::Get()->Queue<KeyCharEvent>( ( uint32_t )wParam );
                return 0;

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                MKT_CORE_LOGGER_INFO( "Mouse Button Down: {}", ( int )msg );
                //EventService::Get()->Queue<MouseButtonPressedEvent>( ( MouseButton )msg, 0 );
                return 0;

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                MKT_CORE_LOGGER_INFO( "Mouse Button Up: {}", ( int )msg );
                //EventService::Get()->Queue<MouseButtonReleasedEvent>( ( MouseButton )msg );
                return 0;

            case WM_MOUSEWHEEL:
                MKT_CORE_LOGGER_INFO( "Mouse Wheel: {}", ( Int32 )GET_WHEEL_DELTA_WPARAM( wParam ) );
                //EventService::Get()->Queue<MouseScrollEvent>( 0.0, ( double )delta / WHEEL_DELTA );
                return 0;

            case WM_MOUSEMOVE:
                double x{ ( double )GET_X_LPARAM( lParam ) };
                double y{ ( double )GET_Y_LPARAM( lParam ) };
                MKT_CORE_LOGGER_INFO( "Mouse Move: {}, {}", x, y );
                //EventService::Get()->Queue<MouseMovedEvent>( x, y );
                return 0;
        }

        return DefWindowProc( hwnd, msg, wParam, lParam );
    }
}// namespace Mikoto

#endif