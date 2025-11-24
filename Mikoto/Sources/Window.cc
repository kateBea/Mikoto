/**
* Window.cc
 * Created by kate on 1/12/24.
 * Updated to use Platform.hh macros
 */

#include <memory>

// Project Headers
#include <Core/Platform.hh>
#include <Platform/MainWindow.hh>

#if defined( MIKOTO_PLATFORM_LINUX )
#include <Platform/LinuxWindow.hh>
#elif defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/Win32Window.hh>
#endif

namespace Mikoto {

    auto Window::Create( const WindowProperties &properties ) -> Window * {

        // Fallback based on platform if no explicit option is set
#if defined( MIKOTO_PLATFORM_WINDOWS ) && defined( MKT_USE_WIN32_WINDOW )
        return new Win32Window( properties );
#elif defined( MIKOTO_PLATFORM_LINUX ) && defined( MKT_USE_XCB_WINDOW )
        return new LinuxWindow( properties );
#else
        return new MainWindow( properties );
#endif
    }

}// namespace Mikoto
