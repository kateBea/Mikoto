//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <SDL3/SDL.h>
#include <EASTL/numeric_limits.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Platform/Event.hh>
#include <Platform/Utility.hh>
#include <Platform/CoreEvents.hh>
#include <Platform/MainWindow.hh>
#include <Platform/EventSystem.hh>
#include <Platform/InputSystem.hh>

#include <Text/Utility.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/Windows/Windows.hh>
#include <dwmapi.h>
#endif

namespace mikoto::platform {

    using namespace mikoto::core;

    static constexpr char kTitleBarProperty[]{ "mikoto.window.title-bar" };

    struct TitleBarState {
        float mX{};
        float mY{};
        float mWidth{};
        float mHeight{};
    };

    static auto SDLCALL DeleteTitleBarState( void*, void* value ) -> void {
        // The SDL property owns this allocation. SDL invokes this cleanup
        // when the property is cleared/replaced or its window is destroyed.
        delete static_cast<TitleBarState*>( value );
    }

    // https://wiki.libsdl.org/SDL3/SDL_SetWindowHitTest
    static auto SDLCALL TitleBarHitTest( SDL_Window* window, const SDL_Point* point, void* data ) -> SDL_HitTestResult {
        // SDL asks what the pointer's location represents. We only classify
        // the area; SDL/the window system handles the actual move or resize.
        // This callback does not track mouse deltas or draw any widgets.
        const auto flags{ SDL_GetWindowFlags( window ) };

        // Fullscreen windows should deliver input to the application rather
        // than offer title-bar dragging or border resizing.
        if ( flags & SDL_WINDOW_FULLSCREEN ) {
            return SDL_HITTEST_NORMAL;
        }

        // Use window coordinates, matching the callback's point, rather
        // than framebuffer pixels (which can differ on high-DPI displays).
        int width{};
        int height{};
        SDL_GetWindowSize( window, &width, &height );

        if ( width <= 0 || height <= 0 ) {
            // Avoid dividing by an invalid window size below.
            return SDL_HITTEST_NORMAL;
        }

        // Resize edges take priority over the draggable title strip. A
        // maximized window has no resize border until it is restored.
        if ( ( flags & SDL_WINDOW_RESIZABLE ) && !( flags & SDL_WINDOW_MAXIMIZED ) ) {
            // Thickness of the invisible resize area, in window coordinates.
            // Six is a usability choice, not an SDL requirement: 5 or 8
            // would also work. Smaller borders require more precise aiming;
            // larger borders take more clickable space away from edge content.
            constexpr int border{ 6 };

            // Pointer coordinates start at the window's top-left corner:
            //
            // (0, 0) --------------------------> x increases
            //   |                            (width, 0)
            //   |
            //   v y increases
            // (0, height)
            //
            // Example: width = 800, height = 600, border = 6.
            //
            // x = 0    6                              794    800
            //     |----|--------------------------------|----|
            //      LEFT          WINDOW INTERIOR         RIGHT
            //
            // Left: x < 6. Right: x >= 800 - 6 (794).
            // Top: y < 6. Bottom: y >= 600 - 6 (594).
            // For example, x = 3 is near the left; x = 798 is near the right.
            // These comparisons only produce booleans; they do not resize.
            //
            // The strips overlap at corners, so (3, 2) is both left and top:
            //
            // +------------+-------------------+-------------+
            // | TOPLEFT    |        TOP        | TOPRIGHT    |
            // +------------+-------------------+-------------+
            // | LEFT       |  WINDOW INTERIOR  | RIGHT       |
            // +------------+-------------------+-------------+
            // | BOTTOMLEFT |      BOTTOM       | BOTTOMRIGHT |
            // +------------+-------------------+-------------+
            const bool left{ point->x < border };
            const bool right{ point->x >= width - border };
            const bool top{ point->y < border };
            const bool bottom{ point->y >= height - border };

            // Test corners along with top/bottom before the side edges so
            // a corner selects diagonal resizing instead of a single axis.
            if ( top ) {
                return left ? SDL_HITTEST_RESIZE_TOPLEFT : right ? SDL_HITTEST_RESIZE_TOPRIGHT
                                                                 : SDL_HITTEST_RESIZE_TOP;
            }

            if ( bottom ) {
                return left ? SDL_HITTEST_RESIZE_BOTTOMLEFT : right ? SDL_HITTEST_RESIZE_BOTTOMRIGHT
                                                                    : SDL_HITTEST_RESIZE_BOTTOM;
            }

            if ( left ) {
                return SDL_HITTEST_RESIZE_LEFT;
            }

            if ( right ) {
                return SDL_HITTEST_RESIZE_RIGHT;
            }
        }

        // data is the same state pointer registered with SDL_SetWindowHitTest.
        // The editor updates its normalized client rectangle each frame,
        // excluding menus/buttons and disabling it while popups are open.
        const auto& area{ *static_cast<const TitleBarState*>( data ) };

        // Convert the pointer to fractions of the window size, matching area.
        // In an 800 x 600 window, (400, 30) becomes (0.5, 0.05).
        const float x{ static_cast<float>( point->x ) / static_cast<float>( width ) };
        const float y{ static_cast<float>( point->y ) / static_cast<float>( height ) };

        // A rectangle is its starting position plus its width and height:
        //
        //                    LEFT                    RIGHT
        //                    area.mX                 area.mX + area.mWidth
        //                       |                         |
        //                       v                         v
        // TOP ----------------> +-------------------------+
        // area.mY               |                         |
        //                       |            * pointer    |
        //                       |                         |
        // BOTTOM -------------> +-------------------------+
        // area.mY + area.mHeight
        //
        // All four comparisons must pass:
        // x >= area.mX                 : at or right of the LEFT edge.
        // x < area.mX + area.mWidth    : before the RIGHT edge.
        // y >= area.mY                 : at or below the TOP edge.
        // y < area.mY + area.mHeight   : above the BOTTOM edge.
        // Screen y increases DOWNWARD. Left/top are included; right/bottom
        // are excluded, so a zero-width or zero-height rectangle is empty.
        //
        // Example using ordinary coordinates to make the bounds concrete:
        //
        //                   x = 20                    x = 80
        //                      |                         |
        //            y = 0 --> +-------------------------+
        //                      |                         |
        //                      |       * (50, 5)         |   * (90, 5)
        //                      |        INSIDE           |    OUTSIDE
        //           y = 10 --> +-------------------------+
        //
        // (50, 5): 20 <= 50 < 80 and 0 <= 5 < 10 -> DRAGGABLE.
        // (90, 5): 90 is past the right edge at 80 -> NORMAL.
        // The code uses normalized fractions, but the comparisons are the
        // same: for an 800 x 600 window, these bounds become x in
        // [20/800, 80/800) and y in [0/600, 10/600).
        //
        // DRAGGABLE authorizes native title-bar interaction at this point;
        // it does not mean a drag has already started. NORMAL leaves input
        // available to the application. A zero-sized area never matches.
        return x >= area.mX && y >= area.mY && x < area.mX + area.mWidth && y < area.mY + area.mHeight
                   ? SDL_HITTEST_DRAGGABLE
                   : SDL_HITTEST_NORMAL;
    }


    MainWindow::MainWindow( const WindowProperties &properties )
        : mTitle{ properties.mTitle },
          mWidth{ properties.mWidth },
          mHeight{ properties.mHeight },
          mBackend{ properties.mBackend },
          mIsResizable{ properties.mResizable },
          mCustomTitleBarRequested{ properties.mCustomTitleBar } {
    }

    MainWindow::~MainWindow() {
        MKT_BEGIN_PROFILER();
        Shutdown();
    }

    auto MainWindow::Init() -> void {
        MKT_BEGIN_PROFILER();

        if ( mWindow ) {
            return;
        }

        SDL_WindowFlags flags{};

        if ( mCustomTitleBarRequested ) {
            // Configure native hit testing and decorations before the first show.
            flags |= SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS;
        }

        if ( mIsResizable ) {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        if ( mBackend == renderer::rhi::GraphicsAPI::eVulkan ) {
            flags |= SDL_WINDOW_VULKAN;
        }

        if ( mBackend == renderer::rhi::GraphicsAPI::eOpenGL ) {
            flags |= SDL_WINDOW_OPENGL;

            if ( !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 ) || 
                 !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 6 ) ||
#if MIKOTO_DEBUG
                 !SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG ) ||
#endif
                 !SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE ) ||
                 !SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 ) ) {
                MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
            }
        }

        mWindow = SDL_CreateWindow( mTitle.c_str(), mWidth, mHeight, flags );

        if ( !mWindow ) {
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }

        if ( mBackend == renderer::rhi::GraphicsAPI::eOpenGL ) {
            mOpenGLContext = SDL_GL_CreateContext( mWindow );
            if ( !mOpenGLContext ) {
                MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
            }
        }

        if ( !SDL_SetPointerProperty( SDL_GetWindowProperties( mWindow ), kOwnerProperty, this ) ) {
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }

        SDL_SetWindowPosition( mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
        SDL_StartTextInput( mWindow );

        core::f32 mouseX{};
        core::f32 mouseY{};

        SDL_GetMouseState( &mouseX, &mouseY );
        mMouseX = mouseX;
        mMouseY = mouseY;

        SDL_GetWindowSizeInPixels( mWindow, &mWidth, &mHeight );

        SetCursorMode( mCursorMode );
        SetCursorType( mCursorType );

        if ( mCustomTitleBarRequested ) {
            if ( !SetCustomTitleBar( true ) && !SDL_SetWindowBordered( mWindow, true ) ) {
                MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
            }

            if ( !SDL_ShowWindow( mWindow ) ) {
                MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
            }
        }
    }

    auto MainWindow::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mWindow ) {
            return;
        }

        if ( platform::InputSystem::HasInstance() ) {
            platform::InputSystem::Get()->ClearFocus( this );
        }

        SDL_ClearProperty( SDL_GetWindowProperties( mWindow ), kOwnerProperty );
        SDL_StopTextInput( mWindow );
        SDL_DestroyCursor( mCursor );
        mCursor = nullptr;

        if ( mOpenGLContext ) {
            SDL_GL_DestroyContext( mOpenGLContext );
            mOpenGLContext = nullptr;
        }

        SDL_DestroyWindow( mWindow );
        mWindow = nullptr;
    }

    auto MainWindow::GetOpenGLContext() -> IOpenGLWindowContext* {
        return mOpenGLContext ? this : nullptr;
    }

    // https://wiki.libsdl.org/SDL3/SDL_GL_MakeCurrent
    auto MainWindow::MakeCurrent() -> bool {
        return mOpenGLContext && SDL_GL_MakeCurrent( mWindow, mOpenGLContext );
    }

    auto MainWindow::GetProcAddress( core::cstr name ) const -> Proc {
        return SDL_GL_GetProcAddress( name );
    }

    auto MainWindow::SwapBuffers() -> bool {
        return SDL_GL_SwapWindow( mWindow );
    }

    auto MainWindow::SetSwapInterval( core::i32 interval ) -> bool {
        return SDL_GL_SetSwapInterval( interval );
    }

    auto MainWindow::ProcessEvents() -> void {
        MKT_BEGIN_PROFILER();
        PollWindowEvents();
    }

    auto MainWindow::PushEvent( const SDL_Event &event ) -> void {
        MKT_BEGIN_PROFILER();
        auto events{ platform::EventSystem::GetPtr() };

        if ( !events || !events->IsInitialized() ) {
            return;
        }

        switch ( event.type ) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                mShouldClose = true;
                events->Queue<platform::WindowCloseEvent>();
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                mWidth = event.window.data1;
                mHeight = event.window.data2;
                events->Queue<platform::WindowResizedEvent>( mWidth, mHeight );
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                mIsMinimized = true;
                events->Queue<platform::WindowResizedEvent>( 0, 0 );
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
                mIsMaximized = true;
                mIsMinimized = false;
                break;
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                // Fullscreen requests may complete asynchronously. SDL has now
                // restored the windowed styles, so restore our DWM policy too.
                RefreshWindowChrome();
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                RefreshWindowChrome();
                mIsMinimized = false;
                mIsMaximized = false;
                SDL_GetWindowSizeInPixels( mWindow, &mWidth, &mHeight );
                events->Queue<platform::WindowResizedEvent>( mWidth, mHeight );
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                if ( platform::InputSystem::HasInstance() ) {
                    platform::InputSystem::Get()->SetFocus( this );
                }

                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if ( platform::InputSystem::HasInstance() ) {
                    platform::InputSystem::Get()->ClearFocus( this );
                }

                break;
            case SDL_EVENT_KEY_DOWN:
                events->Queue<platform::KeyPressedEvent>( FromSDLScancode( event.key.scancode ), event.key.repeat, FromSDLModifiers( event.key.mod ) );
                break;
            case SDL_EVENT_KEY_UP:
                events->Queue<platform::KeyReleasedEvent>( FromSDLScancode( event.key.scancode ) );
                break;
            case SDL_EVENT_TEXT_INPUT:
                if ( event.text.text ) {
                    text::ForEachUtf8CodePoint( event.text.text,
                        [events]( core::u32 point ) {
                        events->Queue<platform::KeyCharEvent>( point );
                    } );
                }

                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                events->Queue<platform::MouseButtonPressedEvent>( FromSDLMouseButton( event.button.button ), FromSDLModifiers( SDL_GetModState() ) );
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                events->Queue<platform::MouseButtonReleasedEvent>( FromSDLMouseButton( event.button.button ) );
                break;
            case SDL_EVENT_MOUSE_MOTION:
                // SDL relative mode reports deltas; accumulate an unbounded virtual
                // position to preserve the camera's existing position-delta API.
                if ( mCursorMode == CursorMode::eDisabled ) {
                    mMouseX += event.motion.xrel;
                    mMouseY += event.motion.yrel;
                }
                else {
                    mMouseX = event.motion.x;
                    mMouseY = event.motion.y;
                }

                events->Queue<platform::MouseMovedEvent>( mMouseX, mMouseY );
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                const core::f32 sign{ event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f };

                if ( auto *input{ platform::InputSystem::GetPtr() } ) {
                    input->AddMouseScroll( this, event.wheel.x * sign, event.wheel.y * sign );
                }

                events->Queue<platform::MouseScrollEvent>( event.wheel.x * sign, event.wheel.y * sign );
                break;
            }

            case SDL_EVENT_DROP_FILE:
                if ( event.drop.data ) {
                    // The engine event copies paths before SDL recycles event storage.
                    core::cstr path{ event.drop.data };
                    events->Queue<platform::ContentDroppedEvent>( 1, &path );
                }

                break;
            default:
                break;
        }
    }

    auto MainWindow::SetCursorPos( core::float2 pos ) -> void {
        SDL_WarpMouseInWindow( mWindow, as<float>( pos.x ), as<float>( pos.y ) );
    }

    auto MainWindow::SetScreenMode( ScreenMode mode ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mode == mScreenMode || !mWindow ) {
            // Avoid issuing a redundant asynchronous SDL request, and do not
            // touch a native handle before the window has been initialized.
            return;
        }

        // SDL separates selecting the display mode from enabling fullscreen.
        // A null mode means "use the desktop's current resolution"; a concrete
        // mode requests an exclusive display mode at those dimensions/refresh.
        const SDL_DisplayMode *displayMode{};

        if ( mode == ScreenMode::eFullScreen ) {
            // Fullscreen uses the monitor currently containing this window, so
            // moving the editor to another monitor selects that monitor's mode.
            displayMode = SDL_GetDesktopDisplayMode( SDL_GetDisplayForWindow( mWindow ) );

            if ( !displayMode ) {
                MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
            }
        }

        // Configure the mode first, then toggle the state. For eBorderless and
        // eFullScreen this enables fullscreen; for eWindowed it disables it.
        // Here displayMode is null for eBorderless, which requests desktop-sized
        // borderless fullscreen. SDL retains the last windowed bounds and can
        // restore them when fullscreen is disabled.
        if ( !SDL_SetWindowFullscreenMode( mWindow, displayMode ) ||
             !SDL_SetWindowFullscreen( mWindow, mode != ScreenMode::eWindowed ) ) {
            // SDL may apply these changes asynchronously, but a false return is
            // an actual failure. Keep mScreenMode unchanged so engine state still
            // describes the previously successful mode.
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }

        // Update the cached state only after both SDL requests succeeded. Resize
        // and fullscreen events will update the pixel dimensions separately.
        mScreenMode = mode;

        // Native decoration preferences are restored by the leave-fullscreen event.
    }

    auto MainWindow::SetCursorMode( CursorMode mode ) -> void {
        MKT_BEGIN_PROFILER();

        const bool capture{ mode == CursorMode::eDisabled };
        const bool wasCaptured{ mCursorMode == CursorMode::eDisabled };

        SDL_Rect captureRect{};

        if ( capture && !wasCaptured ) {
            SDL_GetMouseState( &mCapturedCursorPosition.x, &mCapturedCursorPosition.y );
            captureRect = {
                .x = as<i32>( mCapturedCursorPosition.x ),
                .y = as<i32>( mCapturedCursorPosition.y ),
                .w = 1,
                .h = 1
            };
        }
        else if ( !capture && wasCaptured && SDL_GetKeyboardFocus() == mWindow ) {
            // Restore before leaving relative mode so the warp does not become camera motion.
            SDL_WarpMouseInWindow( mWindow, mCapturedCursorPosition.x, mCapturedCursorPosition.y );
        }

        if ( !SDL_SetWindowRelativeMouseMode( mWindow, capture ) ) {
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }

        // Constrain the cursor while preserving the relative deltas used by the camera.
        // https://github.com/libsdl-org/SDL/blob/release-3.4.18/include/SDL3/SDL_mouse.h
        if ( capture != wasCaptured && !SDL_SetWindowMouseRect( mWindow, capture ? &captureRect : nullptr ) ) {
            const eastl::string error{ SDL_GetError() };
            SDL_SetWindowRelativeMouseMode( mWindow, wasCaptured );
            MKT_THROW_RUNTIME_ERROR( error.c_str() );
        }

        if ( mode == CursorMode::eNormal ) {
            SDL_ShowCursor();
        }
        else {
            SDL_HideCursor();
        }

        if ( mode != CursorMode::eDisabled ) {
            core::f32 mouseX{};
            core::f32 mouseY{};

            SDL_GetMouseState( &mouseX, &mouseY );
            
            mMouseX = mouseX;
            mMouseY = mouseY;
        }

        mCursorMode = mode;
    }

    auto MainWindow::SetCursorType( CursorType type ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mCursor && type == mCursorType ) {
            return;
        }

        SDL_SystemCursor shape{ SDL_SYSTEM_CURSOR_DEFAULT };

        switch ( type ) {
            case CursorType::eHand:
                shape = SDL_SYSTEM_CURSOR_POINTER;
                break;
            case CursorType::eText:
                shape = SDL_SYSTEM_CURSOR_TEXT;
                break;
            case CursorType::eCrossHair:
                shape = SDL_SYSTEM_CURSOR_CROSSHAIR;
                break;
            case CursorType::eResizeHorizontal:
                shape = SDL_SYSTEM_CURSOR_EW_RESIZE;
                break;
            case CursorType::eResizeVertical:
                shape = SDL_SYSTEM_CURSOR_NS_RESIZE;
                break;
            default:
                break;
        }

        auto cursor{ SDL_CreateSystemCursor( shape ) };

        if ( !cursor ) {
            return;
        }

        SDL_SetCursor( cursor );
        SDL_DestroyCursor( mCursor );
        mCursor = cursor;
        mCursorType = type;
    }

    auto MainWindow::ResetCursorType() -> void {
        MKT_BEGIN_PROFILER();
        SetCursorType( CursorType::eArrow );
    }

    auto MainWindow::AllowResizing( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsResizable = value;

        if ( mWindow ) {
            SDL_SetWindowResizable( mWindow, value );
        }
    }

    auto MainWindow::SetWidth( core::i32 width ) -> void {
        MKT_BEGIN_PROFILER();
        core::i32 oldWidth{};
        core::i32 height{};
        SDL_GetWindowSize( mWindow, &oldWidth, &height );

        if ( width > 0 ) {
            SDL_SetWindowSize( mWindow, width, height );
        }
    }

    auto MainWindow::SetHeight( core::i32 height ) -> void {
        MKT_BEGIN_PROFILER();
        core::i32 width{};
        core::i32 oldHeight{};
        SDL_GetWindowSize( mWindow, &width, &oldHeight );

        if ( height > 0 ) {
            SDL_SetWindowSize( mWindow, width, height );
        }
    }

    auto MainWindow::SetIcon( const core::ubyte* data, core::u32 width, core::u32 height ) -> bool {
        // SDL expects tightly packed RGBA8 pixels. Reject invalid input
        // and prevent overflow when computing the four-byte-per-pixel row pitch.
        if ( !data || width <= 0 || height <= 0 || width > eastl::numeric_limits<int>::max() / 4 ) {
            return false;
        }

        // Wrap the caller's pixels without taking ownership. SDL's surface API
        // accepts a mutable pointer, but this path only reads the image data.
        auto *surface{ SDL_CreateSurfaceFrom( 
            width, height, SDL_PIXELFORMAT_RGBA32,
            const_cast<core::ubyte *>( data ), width * 4 ) };

        if ( !surface ) {
            return false;
        }

        // GetNativeWindow stores the backend handle in an any; recover its SDL
        // type here. The icon is copied, so the temporary surface can be freed.
        const bool result{ SDL_SetWindowIcon( mWindow, surface ) };
        SDL_DestroySurface( surface );

        return result;
    }

    auto MainWindow::SetTitle( eastl::string_view title ) -> void {
        MKT_BEGIN_PROFILER();
        mTitle = title;
        SDL_SetWindowTitle( mWindow, mTitle.c_str() );
    }

    auto MainWindow::IsKeyPressed( platform::KeyCode key ) const -> bool {
        MKT_BEGIN_PROFILER();
        core::i32 count{};
        const auto keys{ SDL_GetKeyboardState( &count ) };
        const auto scan{ ConvertToSDLScancode( key ) };
        return SDL_GetKeyboardFocus() == mWindow && scan > SDL_SCANCODE_UNKNOWN && scan < count && keys[scan];
    }

    auto MainWindow::IsKeyReleased( platform::KeyCode key ) const -> bool {
        MKT_BEGIN_PROFILER();
        return SDL_GetKeyboardFocus() == mWindow && ConvertToSDLScancode( key ) != SDL_SCANCODE_UNKNOWN && !IsKeyPressed( key );
    }

    auto MainWindow::IsMouseKeyPressed( platform::MouseButton button ) const -> bool {
        MKT_BEGIN_PROFILER();
        return SDL_GetMouseFocus() == mWindow && ( SDL_GetMouseState( nullptr, nullptr ) & ConvertToSDLMouseButton( button ) ) != 0;
    }

    
    auto MainWindow::GetWidth() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mWidth;
    }

    auto MainWindow::GetHeight() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mHeight;
    }

    auto MainWindow::GetTitle() const -> const eastl::string & {
        MKT_BEGIN_PROFILER();
        return mTitle;
    }

    auto MainWindow::GetScreenMode() const -> ScreenMode {
        MKT_BEGIN_PROFILER();
        return mScreenMode;
    }

    auto MainWindow::IsMinimized() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsMinimized || GetWidth() == 0 || GetHeight() == 0;
    }

    auto MainWindow::IsMaximized() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsMaximized;
    }

    auto MainWindow::IsResizable() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsResizable;
    }

    auto MainWindow::IsApi( renderer::rhi::GraphicsAPI api ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mBackend == api;
    }

    auto MainWindow::GetApi() const -> renderer::rhi::GraphicsAPI {
        MKT_BEGIN_PROFILER();
        return mBackend;
    }

    auto MainWindow::IsCursorMode( const CursorMode mode ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mCursorMode == mode;
    }

    auto MainWindow::IsCursorType( const CursorType type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mCursorType == type;
    }

    auto MainWindow::GetCursorMode() const -> CursorMode {
        MKT_BEGIN_PROFILER();
        return mCursorMode;
    }

    auto MainWindow::GetCursorType() const -> CursorType {
        MKT_BEGIN_PROFILER();
        return mCursorType;
    }

    auto MainWindow::SetCustomTitleBar( bool enabled ) -> bool {
        if ( !mWindow ) {
            return false;
        }

        // SDL owns the drag rectangle for this window.
        const auto properties{ SDL_GetWindowProperties( mWindow ) };

        if ( !enabled ) {
#if defined( MIKOTO_PLATFORM_WINDOWS )
            // Restore the OS corner policy when returning to native chrome.
            // DWM controls Windows decoration; it is not needed for SDL dragging.
            const auto native{ static_cast<HWND>( GetSystemHandle() ) };
            const DWM_WINDOW_CORNER_PREFERENCE corners{ DWMWCP_DEFAULT };
            DwmSetWindowAttribute( native, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof( corners ) );
#endif

            // Detach the callback before clearing its owned state, then restore
            // the native border/title bar. Clearing the property deletes state.
            SDL_SetWindowHitTest( mWindow, nullptr, nullptr );
            SDL_ClearProperty( properties, kTitleBarProperty );
            return SDL_SetWindowBordered( mWindow, true );
        }

        if ( SDL_GetPointerProperty( properties, kTitleBarProperty, nullptr ) ) {
            // Keep the callback/drag rectangle, but SDL may have reset native styling.
            RefreshWindowChrome();
            return true;
        }

        // Initially the rectangle is empty. The editor supplies its usable title
        // area after laying out the menus and window controls.
        auto* state{ new TitleBarState{} };

        // Transfer ownership to SDL. It calls cleanup even if setting this
        // property fails, so manually deleting state here would double-free it.
        if ( !SDL_SetPointerPropertyWithCleanup( properties, kTitleBarProperty, state, DeleteTitleBarState, nullptr ) ) {
            return false;
        }

        // Install native hit testing before removing decorations. The editor
        // draws the replacement title bar; SDL supplies window interaction.
        if ( !SDL_SetWindowHitTest( mWindow, TitleBarHitTest, state ) || !SDL_SetWindowBordered( mWindow, false ) ) {
            // Roll back the callback/property if either operation fails. The
            // caller can restore native decorations when we report failure.
            SDL_SetWindowHitTest( mWindow, nullptr, nullptr );
            SDL_ClearProperty( properties, kTitleBarProperty );
            return false;
        }

        RefreshWindowChrome();
        return true;
    }

    auto MainWindow::RefreshWindowChrome() -> void {
#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( !mWindow || ( SDL_GetWindowFlags( mWindow ) & SDL_WINDOW_FULLSCREEN ) ) {
            return;
        }

        const auto native{ static_cast<HWND>( GetSystemHandle() ) };

        if ( !native ) {
            return;
        }

        const DWM_WINDOW_CORNER_PREFERENCE corners{
            HasCustomTitleBar() ? DWMWCP_ROUND : DWMWCP_DEFAULT
        };
        // Optional on older Windows. Maximized and snapped windows remain square.
        DwmSetWindowAttribute( native, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof( corners ) );
#endif
    }

    auto MainWindow::SetTitleBarDragArea( core::f32 x, core::f32 y, core::f32 width, core::f32 height ) -> void {
        if ( !mWindow ) {
            return;
        }

        // The editor converts its layout to fractions of the client width/height.
        // Update the state in place: the hit-test callback already points at it,
        // so it does not need to be reinstalled as the layout changes.
        auto* state{ static_cast<TitleBarState*>( SDL_GetPointerProperty( SDL_GetWindowProperties( mWindow ), kTitleBarProperty, nullptr ) ) };

        if ( state ) {
            // No state means custom chrome is inactive. An empty rectangle is
            // how the editor temporarily disables dragging, e.g. for a popup.
            *state = { x, y, width, height };
        }
    }

    auto MainWindow::Minimize() -> void {
        // Backend events update the window state after the native request.
        if ( mWindow ) {
            SDL_MinimizeWindow( mWindow );
        }
    }

    auto MainWindow::ToggleMaximized() -> void {
        if ( !mWindow ) {
            return;
        }

        // Query SDL so changes initiated by the window manager are reflected.
        if ( SDL_GetWindowFlags( mWindow ) & SDL_WINDOW_MAXIMIZED ) {
            SDL_RestoreWindow( mWindow );
        }
        else {
            SDL_MaximizeWindow( mWindow );
        }
    }

    auto MainWindow::RequestClose() -> void {
        if ( !mWindow ) {
            return;
        }

        // Use the same event route as the native close button. MainWindow turns
        // this into a WindowCloseEvent, and EditorApp handles stopping the app.
        // Do not destroy the window here while the UI frame is being built.
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        // Identify the recipient so SDL event dispatch reaches the right window.
        event.window.windowID = SDL_GetWindowID( mWindow );
        SDL_PushEvent( &event );
    }

    auto MainWindow::HasCustomTitleBar() const -> bool {
        MKT_BEGIN_PROFILER();
        return mWindow && SDL_GetPointerProperty( SDL_GetWindowProperties( mWindow ), kTitleBarProperty, nullptr ) != nullptr;
    }

    auto MainWindow::IsScreenMode( ScreenMode mode ) const -> bool {
        return mScreenMode == mode;
    }

    auto MainWindow::IsMouseKeyReleased( platform::MouseButton button ) const -> bool {
        MKT_BEGIN_PROFILER();
        return SDL_GetMouseFocus() == mWindow && ConvertToSDLMouseButton( button ) != 0 && !IsMouseKeyPressed( button );
    }

    auto MainWindow::GetMousePos() const -> eastl::pair<core::f64, core::f64> {
        MKT_BEGIN_PROFILER();
        return { mMouseX, mMouseY };
    }

    auto MainWindow::GetMouseX() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return mMouseX;
    }

    auto MainWindow::GetMouseY() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return mMouseY;
    }

    auto MainWindow::GetClipBoardContents() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        auto contents{ SDL_GetClipboardText() };
        eastl::string result{ contents ? contents : "" };
        SDL_free( contents );
        return result;
    }

    auto MainWindow::ShouldClose() const -> bool {
        MKT_BEGIN_PROFILER();
        return mShouldClose;
    }

    auto MainWindow::GetSystemHandle() const -> void* {
        MKT_BEGIN_PROFILER();

#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( mWindow ) {
            return SDL_GetPointerProperty( SDL_GetWindowProperties( mWindow ), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr );
        }
#endif

        return nullptr;
    }

    auto MainWindow::GetNativeWindow() const -> eastl::any {
        MKT_BEGIN_PROFILER();
        return mWindow;
    }
}// namespace mikoto::platform
