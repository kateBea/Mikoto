/**
 * LinuxWindow.cc
 * Created by kate on 11/24/25.
 * */

#include <array>
#include <string_view>

#include <Common/Common.hh>
#include <Logging/Logger.hh>
#include <Platform/LinuxWindow.hh>

#if defined(MIKOTO_PLATFORM_LINUX) && defined(MKT_USE_XCB_WINDOW)

namespace Mikoto {

    LinuxWindow::LinuxWindow( const WindowProperties& props )
        : Window( props ) {
    }

    LinuxWindow::~LinuxWindow() {
        Shutdown();
    }

    auto LinuxWindow::Init() -> void {
        m_Connection = xcb_connect( nullptr, nullptr );
        if ( xcb_connection_has_error( m_Connection ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to connect to X server via XCB" );
        }

        const xcb_setup_t* setup{ xcb_get_setup( m_Connection ) };
        xcb_screen_iterator_t iter{ xcb_setup_roots_iterator( setup ) };
        m_Screen = iter.data;

        m_Window = xcb_generate_id( m_Connection );
        UInt32 mask{ XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK };
        std::array<UInt32, 2> values{};
        values[0] = m_Screen->black_pixel;
        values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

        xcb_create_window(
            m_Connection,
            XCB_COPY_FROM_PARENT,
            m_Window,
            m_Screen->root,
            0, 0,
            m_Properties.Width, m_Properties.Height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            m_Screen->root_visual,
            mask, values.data()
        );

        // Set title
        xcb_change_property(
            m_Connection,
            XCB_PROP_MODE_REPLACE,
            m_Window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            m_Properties.Title.length(),
            m_Properties.Title.c_str()
        );

        // Handle delete window atom
        constexpr std::string_view atom{ "WM_DELETE_WINDOW" };
        xcb_intern_atom_cookie_t cookie{ xcb_intern_atom( m_Connection, 1, atom.size(), atom.data() ) };
        m_AtomWmDeleteWindow = xcb_intern_atom_reply( m_Connection, cookie, 0 );

        // We also need WM_PROTOCOLS
        xcb_intern_atom_cookie_t protoCookie = xcb_intern_atom( m_Connection, 1, 12, "WM_PROTOCOLS" );
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply( m_Connection, protoCookie, 0 );

        std::array atoms{ m_AtomWmDeleteWindow->atom };

        xcb_change_property(
            m_Connection,
            XCB_PROP_MODE_REPLACE,
            m_Window,
            reply->atom,
            XCB_ATOM_ATOM,
            32,
            atoms.size(),
            atoms.data()
        );
        free(reply);

        xcb_map_window( m_Connection, m_Window );
        xcb_flush( m_Connection );
    }

    auto LinuxWindow::Shutdown() -> void {
        if ( m_AtomWmDeleteWindow ) {
            free( m_AtomWmDeleteWindow );
            m_AtomWmDeleteWindow = nullptr;
        }
        if ( m_Connection ) {
            xcb_destroy_window( m_Connection, m_Window );
            xcb_disconnect( m_Connection );
            m_Connection = nullptr;
            m_Window = 0;
        }
    }

    auto LinuxWindow::ProcessEvents() -> void {
        xcb_generic_event_t* event{};

        while ( m_Connection != nullptr && ( event = xcb_poll_for_event( m_Connection ) ) ) {
            switch ( event->response_type & ~0x80 ) {
                case XCB_CLIENT_MESSAGE: {
                    // Cast the generic XCB event pointer to a client message event pointer
                    // XCB_CLIENT_MESSAGE events are sent by the window manager, e.g., when the user clicks the "X" button
                    auto* cm{ reinterpret_cast<xcb_client_message_event_t*>( event ) };

                    // Check if this client message corresponds to our "window close" request
                    // m_AtomWmDeleteWindow is the atom we obtained during initialization that represents WM_DELETE_WINDOW
                    // The data32[0] field contains the atom of the message being sent
                    if ( cm->data.data32[0] == m_AtomWmDeleteWindow->atom ) {

                        m_ShouldClose = true;
                    }
                    break;
                }
                case XCB_CONFIGURE_NOTIFY: {
                    auto* cn{ reinterpret_cast<xcb_configure_notify_event_t*>( event ) };
                    m_Properties.Width = cn->width;
                    m_Properties.Height = cn->height;
                    break;
                }
            }
            free( event );
        }
    }

    auto LinuxWindow::GetNativeWindow() const -> std::any {
        return m_Window;
    }

    auto LinuxWindow::SetScreenMode( ScreenMode mode ) -> void {
        m_ScreenMode = mode;
    }

    auto LinuxWindow::IsKeyPressed( KeyCode keyCode ) const -> bool {
        return false;
    }

    auto LinuxWindow::IsKeyReleased( KeyCode keyCode ) const -> bool {
        return false;
    }

    auto LinuxWindow::IsMouseKeyPressed( MouseButton button ) const -> bool {
        return false;
    }

    auto LinuxWindow::IsMouseKeyReleased( MouseButton button ) const -> bool {
        return false;
    }

    auto LinuxWindow::GetMouseX() const -> double {
        auto [x, y] = GetMousePos();
        return x;
    }

    auto LinuxWindow::GetMouseY() const -> double {
        auto [x, y] = GetMousePos();
        return y;
    }

    auto LinuxWindow::GetMousePos() const -> std::pair<double, double> {
        if ( !m_Connection ) return { 0.0, 0.0 };

        xcb_query_pointer_cookie_t cookie{ xcb_query_pointer( m_Connection, m_Window ) };
        xcb_query_pointer_reply_t* reply{ xcb_query_pointer_reply( m_Connection, cookie, 0 )  };

        if ( reply ) {
            double x = reply->win_x;
            double y = reply->win_y;
            free( reply );
            return { x, y };
        }
        return { 0.0, 0.0 };
    }

    auto LinuxWindow::ShouldClose() const -> bool {
        return m_ShouldClose;
    }

} // namespace Mikoto

#endif