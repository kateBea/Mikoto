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

#include <filesystem>

#include <Core/Platform.hh>
#include <Core/Profiler.hh>

#include <Platform/Utility.hh>
#include <Platform/KeyCodes.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Platform/Windows/Windows.hh>

#include <dwrite.h>
#include <wrl/client.h>

#endif

#if MIKOTO_MODULE_RENDERER
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Platform/Window.hh>

#endif // MIKOTO_MODULE_RENDERER

namespace mikoto::platform {

#if defined( MIKOTO_PLATFORM_WINDOWS )
    using Microsoft::WRL::ComPtr;

    // DirectWrite resolves each face to its local file and index within a collection.
    // https://learn.microsoft.com/en-us/windows/win32/directwrite/font-selection
    static auto GetFontFilePath( IDWriteFontFace& face ) -> filesystem::Path {
        core::u32 count{};
        if ( FAILED( face.GetFiles( &count, nullptr ) ) || count != 1 ) {
            return {};
        }

        ComPtr<IDWriteFontFile> file{};
        ComPtr<IDWriteFontFileLoader> loader{};
        ComPtr<IDWriteLocalFontFileLoader> localLoader{};
        if ( FAILED( face.GetFiles( &count, file.GetAddressOf() ) ) ||
             FAILED( file->GetLoader( loader.GetAddressOf() ) ) ||
             FAILED( loader.As( &localLoader ) ) ) {
            return {};
        }

        const void* key{};
        core::u32 keySize{};
        core::u32 length{};
        if ( FAILED( file->GetReferenceKey( &key, &keySize ) ) ||
             FAILED( localLoader->GetFilePathLengthFromKey( key, keySize, &length ) ) ) {
            return {};
        }

        eastl::wstring path{};
        path.resize( length + 1 );
        if ( FAILED( localLoader->GetFilePathFromKey( key, keySize, path.data(), length + 1 ) ) ) {
            return {};
        }

        return filesystem::Path{ std::filesystem::path{ path.c_str() } };
    }

    static auto GetFontFamilyName( IDWriteFontFamily& family ) -> eastl::string {
        ComPtr<IDWriteLocalizedStrings> names{};
        if ( FAILED( family.GetFamilyNames( names.GetAddressOf() ) ) || names->GetCount() == 0 ) {
            return {};
        }

        // Prefer an invariant name so saved preferences survive a Windows language change.
        core::u32 index{};
        BOOL found{};
        if ( FAILED( names->FindLocaleName( L"en-us", &index, &found ) ) || !found ) {
            index = 0;
        }

        core::u32 length{};
        if ( FAILED( names->GetStringLength( index, &length ) ) ) {
            return {};
        }

        eastl::wstring name{};
        name.resize( length + 1 );
        if ( FAILED( names->GetString( index, name.data(), length + 1 ) ) ) {
            return {};
        }

        const auto count{ core::as<core::i32>( length ) };
        const auto bytes{ WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, name.data(), count, nullptr, 0, nullptr, nullptr ) };
        if ( bytes <= 0 ) {
            return {};
        }

        eastl::string utf8{};
        utf8.resize( core::as<core::usize>( bytes ) );
        if ( !WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, name.data(), count, utf8.data(), bytes, nullptr, nullptr ) ) {
            return {};
        }

        return utf8;
    }
#endif

    auto GetInstalledFontFamilies() -> eastl::vector<InstalledFontFamily> {
        eastl::vector<InstalledFontFamily> families{};

#if defined( MIKOTO_PLATFORM_WINDOWS )
        // https://learn.microsoft.com/en-us/windows/win32/directwrite/font-enumeration
        ComPtr<IDWriteFactory> factory{};
        ComPtr<IDWriteFontCollection> collection{};
        if ( FAILED( DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof( IDWriteFactory ),
                 reinterpret_cast<IUnknown**>( factory.GetAddressOf() ) ) ) ||
             FAILED( factory->GetSystemFontCollection( collection.GetAddressOf() ) ) ) {
            return families;
        }

        const core::u32 count{ collection->GetFontFamilyCount() };
        families.reserve( count );

        for ( core::u32 index{}; index < count; ++index ) {
            ComPtr<IDWriteFontFamily> family{};
            ComPtr<IDWriteFont> font{};
            if ( FAILED( collection->GetFontFamily( index, family.GetAddressOf() ) ) ||
                 FAILED( family->GetFirstMatchingFont( DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                     DWRITE_FONT_STYLE_NORMAL, font.GetAddressOf() ) ) ) {
                continue;
            }

            BOOL hasLatin{};
            ComPtr<IDWriteFontFace> face{};
            if ( FAILED( font->HasCharacter( 'A', &hasLatin ) ) || !hasLatin ||
                 FAILED( font->CreateFontFace( face.GetAddressOf() ) ) ) {
                continue;
            }

            const auto type{ face->GetType() };
            if ( type != DWRITE_FONT_FACE_TYPE_TRUETYPE && type != DWRITE_FONT_FACE_TYPE_CFF &&
                 type != DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION ) {
                continue;
            }

            auto name{ GetFontFamilyName( *family.Get() ) };
            auto path{ GetFontFilePath( *face.Get() ) };
            if ( !name.empty() && path.IsFile() ) {
                families.push_back( {
                    .mFamily = eastl::move( name ),
                    .mPath = eastl::move( path ),
                    .mFaceIndex = face->GetIndex()
                } );
            }
        }
#endif

        return families;
    }

    namespace windows {

#if defined( MIKOTO_PLATFORM_WINDOWS )
        using namespace mikoto::core;

        auto GuidToString( const GUID& guid ) -> eastl::string {
            MKT_BEGIN_PROFILER();
            eastl::array<wchar_t, 256> buffer{};
            const int written{ StringFromGUID2( guid, buffer.data(), core::as<core::i32>( buffer.size() ) ) };

            if ( written == 0 ) {
                return "";
            }

            return string::FromWChar( buffer.data() );
        }
#endif

    }// namespace windows

    auto GetStringRepresentation( KeyCode code ) -> eastl::string_view {
        MKT_BEGIN_PROFILER();

        switch ( code ) {
            case Key_Space:
                return KEY_STRING_REPRESENTATION( MKT_KEY_SPACE );
            case Key_Apostrophe:
                return KEY_STRING_REPRESENTATION( MKT_KEY_APOSTROPHE );
            case Key_Comma:
                return KEY_STRING_REPRESENTATION( MKT_KEY_COMMA );
            case Key_Minus:
                return KEY_STRING_REPRESENTATION( MKT_KEY_MINUS );
            case Key_Period:
                return KEY_STRING_REPRESENTATION( MKT_KEY_PERIOD );
            case Key_Slash:
                return KEY_STRING_REPRESENTATION( MKT_KEY_SLASH );
            case Key_0:
                return KEY_STRING_REPRESENTATION( MKT_KEY_0 );
            case Key_1:
                return KEY_STRING_REPRESENTATION( MKT_KEY_1 );
            case Key_2:
                return KEY_STRING_REPRESENTATION( MKT_KEY_2 );
            case Key_3:
                return KEY_STRING_REPRESENTATION( MKT_KEY_3 );
            case Key_4:
                return KEY_STRING_REPRESENTATION( MKT_KEY_4 );
            case Key_5:
                return KEY_STRING_REPRESENTATION( MKT_KEY_5 );
            case Key_6:
                return KEY_STRING_REPRESENTATION( MKT_KEY_6 );
            case Key_7:
                return KEY_STRING_REPRESENTATION( MKT_KEY_7 );
            case Key_8:
                return KEY_STRING_REPRESENTATION( MKT_KEY_8 );
            case Key_9:
                return KEY_STRING_REPRESENTATION( MKT_KEY_9 );

            case Key_Semicolon:
                return KEY_STRING_REPRESENTATION( MKT_KEY_SEMICOLON );
            case Key_Equal:
                return KEY_STRING_REPRESENTATION( MKT_KEY_EQUAL );

            case Key_A:
                return KEY_STRING_REPRESENTATION( MKT_KEY_A );
            case Key_B:
                return KEY_STRING_REPRESENTATION( MKT_KEY_B );
            case Key_C:
                return KEY_STRING_REPRESENTATION( MKT_KEY_C );
            case Key_D:
                return KEY_STRING_REPRESENTATION( MKT_KEY_D );
            case Key_E:
                return KEY_STRING_REPRESENTATION( MKT_KEY_E );
            case Key_F:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F );
            case Key_G:
                return KEY_STRING_REPRESENTATION( MKT_KEY_G );
            case Key_H:
                return KEY_STRING_REPRESENTATION( MKT_KEY_H );
            case Key_I:
                return KEY_STRING_REPRESENTATION( MKT_KEY_I );
            case Key_J:
                return KEY_STRING_REPRESENTATION( MKT_KEY_J );
            case Key_K:
                return KEY_STRING_REPRESENTATION( MKT_KEY_K );
            case Key_L:
                return KEY_STRING_REPRESENTATION( MKT_KEY_L );
            case Key_M:
                return KEY_STRING_REPRESENTATION( MKT_KEY_M );
            case Key_N:
                return KEY_STRING_REPRESENTATION( MKT_KEY_N );
            case Key_O:
                return KEY_STRING_REPRESENTATION( MKT_KEY_O );
            case Key_P:
                return KEY_STRING_REPRESENTATION( MKT_KEY_P );
            case Key_Q:
                return KEY_STRING_REPRESENTATION( MKT_KEY_Q );
            case Key_R:
                return KEY_STRING_REPRESENTATION( MKT_KEY_R );
            case Key_S:
                return KEY_STRING_REPRESENTATION( MKT_KEY_S );
            case Key_T:
                return KEY_STRING_REPRESENTATION( MKT_KEY_T );
            case Key_U:
                return KEY_STRING_REPRESENTATION( MKT_KEY_U );
            case Key_V:
                return KEY_STRING_REPRESENTATION( MKT_KEY_V );
            case Key_W:
                return KEY_STRING_REPRESENTATION( MKT_KEY_W );
            case Key_X:
                return KEY_STRING_REPRESENTATION( MKT_KEY_X );
            case Key_Y:
                return KEY_STRING_REPRESENTATION( MKT_KEY_Y );
            case Key_Z:
                return KEY_STRING_REPRESENTATION( MKT_KEY_Z );
            case Key_Left_Bracket:
                return KEY_STRING_REPRESENTATION( MKT_KEY_LEFT_BRACKET );
            case Key_Backslash:
                return KEY_STRING_REPRESENTATION( MKT_KEY_BACKSLASH );
            case Key_Right_Bracket:
                return KEY_STRING_REPRESENTATION( MKT_KEY_RIGHT_BRACKET );
            case Key_Grave_Accent:
                return KEY_STRING_REPRESENTATION( MKT_KEY_GRAVE_ACCENT );
            case Key_World_1:
                return KEY_STRING_REPRESENTATION( MKT_KEY_WORLD_1 );
            case Key_World_2:
                return KEY_STRING_REPRESENTATION( MKT_KEY_WORLD_2 );

            case Key_Escape:
                return KEY_STRING_REPRESENTATION( MKT_KEY_ESCAPE );
            case Key_Enter:
                return KEY_STRING_REPRESENTATION( MKT_KEY_ENTER );
            case Key_Tab:
                return KEY_STRING_REPRESENTATION( MKT_KEY_TAB );
            case Key_Backspace:
                return KEY_STRING_REPRESENTATION( MKT_KEY_BACKSPACE );
            case Key_Insert:
                return KEY_STRING_REPRESENTATION( MKT_KEY_INSERT );
            case Key_Delete:
                return KEY_STRING_REPRESENTATION( MKT_KEY_DELETE );
            case Key_Right:
                return KEY_STRING_REPRESENTATION( MKT_KEY_RIGHT );
            case Key_Left:
                return KEY_STRING_REPRESENTATION( MKT_KEY_LEFT );
            case Key_Down:
                return KEY_STRING_REPRESENTATION( MKT_KEY_DOWN );
            case Key_Up:
                return KEY_STRING_REPRESENTATION( MKT_KEY_UP );
            case Key_Page_Up:
                return KEY_STRING_REPRESENTATION( MKT_KEY_PAGE_UP );
            case Key_Page_Down:
                return KEY_STRING_REPRESENTATION( MKT_KEY_PAGE_DOWN );
            case Key_Home:
                return KEY_STRING_REPRESENTATION( MKT_KEY_HOME );
            case Key_End:
                return KEY_STRING_REPRESENTATION( MKT_KEY_END );
            case Key_Caps_Lock:
                return KEY_STRING_REPRESENTATION( MKT_KEY_CAPS_LOCK );
            case Key_Scroll_Lock:
                return KEY_STRING_REPRESENTATION( MKT_KEY_SCROLL_LOCK );
            case Key_Num_Lock:
                return KEY_STRING_REPRESENTATION( MKT_KEY_NUM_LOCK );
            case Key_Print_Screen:
                return KEY_STRING_REPRESENTATION( MKT_KEY_PRINT_SCREEN );
            case Key_Pause:
                return KEY_STRING_REPRESENTATION( MKT_KEY_PAUSE );

            case Key_F1:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F1 );
            case Key_F2:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F2 );
            case Key_F3:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F3 );
            case Key_F4:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F4 );
            case Key_F5:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F5 );
            case Key_F6:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F6 );
            case Key_F7:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F7 );
            case Key_F8:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F8 );
            case Key_F9:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F9 );
            case Key_F10:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F10 );
            case Key_F11:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F11 );
            case Key_F12:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F12 );
            case Key_F13:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F13 );
            case Key_F14:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F14 );
            case Key_F15:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F15 );
            case Key_F16:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F16 );
            case Key_F17:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F17 );
            case Key_F18:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F18 );
            case Key_F19:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F19 );
            case Key_F20:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F20 );
            case Key_F21:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F21 );
            case Key_F22:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F22 );
            case Key_F23:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F23 );
            case Key_F24:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F24 );
            case Key_F25:
                return KEY_STRING_REPRESENTATION( MKT_KEY_F25 );

            case Key_KP_0:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_0 );
            case Key_KP_1:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_1 );
            case Key_KP_2:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_2 );
            case Key_KP_3:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_3 );
            case Key_KP_4:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_4 );
            case Key_KP_5:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_5 );
            case Key_KP_6:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_6 );
            case Key_KP_7:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_7 );
            case Key_KP_8:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_8 );
            case Key_KP_9:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_9 );
            case Key_KP_Decimal:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_DECIMAL );
            case Key_KP_Divide:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_DIVIDE );
            case Key_KP_Multiply:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_MULTIPLY );
            case Key_KP_Subtract:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_SUBTRACT );
            case Key_KP_Add:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_ADD );
            case Key_KP_Enter:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_ENTER );
            case Key_KP_Equal:
                return KEY_STRING_REPRESENTATION( MKT_KEY_KP_EQUAL );

            case Key_Left_Shift:
                return KEY_STRING_REPRESENTATION( MKT_KEY_LEFT_SHIFT );
            case Key_Left_Control:
                return KEY_STRING_REPRESENTATION( MKT_KEY_LEFT_CONTROL );
            case Key_Left_Alt:
                return KEY_STRING_REPRESENTATION( MKT_KEY_LEFT_ALT );
            case Key_Left_Super:
                return KEY_STRING_REPRESENTATION( MKT_KEY_LEFT_SUPER );
            case Key_Right_Shift:
                return KEY_STRING_REPRESENTATION( MKT_KEY_RIGHT_SHIFT );
            case Key_Right_Control:
                return KEY_STRING_REPRESENTATION( MKT_KEY_RIGHT_CONTROL );
            case Key_Right_Alt:
                return KEY_STRING_REPRESENTATION( MKT_KEY_RIGHT_ALT );
            case Key_Right_Super:
                return KEY_STRING_REPRESENTATION( MKT_KEY_RIGHT_SUPER );
            case Key_Menu:
                return KEY_STRING_REPRESENTATION( MKT_KEY_MENU );
            default:
                return "Unknown key code";
        }
    }

    auto operator|( ModKey a, ModKey b ) -> ModKey {
        MKT_BEGIN_PROFILER();
        return static_cast<ModKey>( static_cast<core::i32>( a ) | static_cast<core::i32>( b ) );
    }

    auto operator&( ModKey a, ModKey b ) -> ModKey {
        MKT_BEGIN_PROFILER();
        return static_cast<ModKey>( static_cast<core::i32>( a ) & static_cast<core::i32>( b ) );
    }

    auto GetStringRepresentation( MouseButton button ) -> eastl::string_view {
        MKT_BEGIN_PROFILER();

        switch ( button ) {
            case Mouse_Button_1:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_LEFT );
            case Mouse_Button_2:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_RIGHT );
            case Mouse_Button_3:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_MIDDLE );
            case Mouse_Button_4:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_4 );
            case Mouse_Button_5:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_5 );
            case Mouse_Button_6:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_6 );
            case Mouse_Button_7:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_7 );
            case Mouse_Button_8:
                return MOUSE_BUTTON_STRING_REPRESENTATION( MKT_MOUSE_BUTTON_8 );
            default:
                return "Unknown mouse button";
        }
    }


#if MIKOTO_MODULE_RENDERER
    auto InitializeWindowBackend() -> void {
        MKT_BEGIN_PROFILER();
        if ( !SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) ) {
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }
    }

    auto ShutdownWindowBackend() -> void {
        MKT_BEGIN_PROFILER();
        SDL_QuitSubSystem( SDL_INIT_VIDEO | SDL_INIT_EVENTS );
    }

    auto GetWindowVulkanExtensions() -> eastl::vector<core::cstr> {
        MKT_BEGIN_PROFILER();
        core::u32 count{};
        // SDL loads the Vulkan loader when creating an SDL_WINDOW_VULKAN window.
        const auto names{ SDL_Vulkan_GetInstanceExtensions( &count ) };
        if ( !names || count == 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Window backend cannot provide Vulkan surface extensions" );
        }

        return { names, names + count };
    }

    auto CreateWindowVulkanSurface( const IWindow& window, VkInstance instance, VkSurfaceKHR* surface ) -> VkResult {
        MKT_BEGIN_PROFILER();
        if ( !SDL_Vulkan_CreateSurface( eastl::any_cast<SDL_Window*>( window.GetNativeWindow() ), instance, nullptr, surface ) ) {
            MKT_CORE_LOGGER_ERROR( "SDL Vulkan surface creation failed: {}", SDL_GetError() );
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        return VK_SUCCESS;
    }

#endif

#if MIKOTO_MODULE_RENDERER
    // Physical keys use US-layout positions for stable engine bindings. Text input is
    // decoded separately from SDL_TEXTINPUT, so Unicode/layout text is unaffected.
    auto ConvertToSDLScancode( KeyCode key ) -> SDL_Scancode {
        MKT_BEGIN_PROFILER();

        switch ( key ) {
            case Key_Space:
                return SDL_SCANCODE_SPACE;
            case Key_Apostrophe:
                return SDL_SCANCODE_APOSTROPHE;
            case Key_Comma:
                return SDL_SCANCODE_COMMA;
            case Key_Minus:
                return SDL_SCANCODE_MINUS;
            case Key_Period:
                return SDL_SCANCODE_PERIOD;
            case Key_Slash:
                return SDL_SCANCODE_SLASH;
            case Key_0:
                return SDL_SCANCODE_0;
            case Key_1:
                return SDL_SCANCODE_1;
            case Key_2:
                return SDL_SCANCODE_2;
            case Key_3:
                return SDL_SCANCODE_3;
            case Key_4:
                return SDL_SCANCODE_4;
            case Key_5:
                return SDL_SCANCODE_5;
            case Key_6:
                return SDL_SCANCODE_6;
            case Key_7:
                return SDL_SCANCODE_7;
            case Key_8:
                return SDL_SCANCODE_8;
            case Key_9:
                return SDL_SCANCODE_9;
            case Key_Semicolon:
                return SDL_SCANCODE_SEMICOLON;
            case Key_Equal:
                return SDL_SCANCODE_EQUALS;
            case Key_A:
                return SDL_SCANCODE_A;
            case Key_B:
                return SDL_SCANCODE_B;
            case Key_C:
                return SDL_SCANCODE_C;
            case Key_D:
                return SDL_SCANCODE_D;
            case Key_E:
                return SDL_SCANCODE_E;
            case Key_F:
                return SDL_SCANCODE_F;
            case Key_G:
                return SDL_SCANCODE_G;
            case Key_H:
                return SDL_SCANCODE_H;
            case Key_I:
                return SDL_SCANCODE_I;
            case Key_J:
                return SDL_SCANCODE_J;
            case Key_K:
                return SDL_SCANCODE_K;
            case Key_L:
                return SDL_SCANCODE_L;
            case Key_M:
                return SDL_SCANCODE_M;
            case Key_N:
                return SDL_SCANCODE_N;
            case Key_O:
                return SDL_SCANCODE_O;
            case Key_P:
                return SDL_SCANCODE_P;
            case Key_Q:
                return SDL_SCANCODE_Q;
            case Key_R:
                return SDL_SCANCODE_R;
            case Key_S:
                return SDL_SCANCODE_S;
            case Key_T:
                return SDL_SCANCODE_T;
            case Key_U:
                return SDL_SCANCODE_U;
            case Key_V:
                return SDL_SCANCODE_V;
            case Key_W:
                return SDL_SCANCODE_W;
            case Key_X:
                return SDL_SCANCODE_X;
            case Key_Y:
                return SDL_SCANCODE_Y;
            case Key_Z:
                return SDL_SCANCODE_Z;
            case Key_Left_Bracket:
                return SDL_SCANCODE_LEFTBRACKET;
            case Key_Backslash:
                return SDL_SCANCODE_BACKSLASH;
            case Key_Right_Bracket:
                return SDL_SCANCODE_RIGHTBRACKET;
            case Key_Grave_Accent:
                return SDL_SCANCODE_GRAVE;
            case Key_World_1:
                return SDL_SCANCODE_INTERNATIONAL1;
            case Key_World_2:
                return SDL_SCANCODE_NONUSBACKSLASH;
            case Key_Escape:
                return SDL_SCANCODE_ESCAPE;
            case Key_Enter:
                return SDL_SCANCODE_RETURN;
            case Key_Tab:
                return SDL_SCANCODE_TAB;
            case Key_Backspace:
                return SDL_SCANCODE_BACKSPACE;
            case Key_Insert:
                return SDL_SCANCODE_INSERT;
            case Key_Delete:
                return SDL_SCANCODE_DELETE;
            case Key_Right:
                return SDL_SCANCODE_RIGHT;
            case Key_Left:
                return SDL_SCANCODE_LEFT;
            case Key_Down:
                return SDL_SCANCODE_DOWN;
            case Key_Up:
                return SDL_SCANCODE_UP;
            case Key_Page_Up:
                return SDL_SCANCODE_PAGEUP;
            case Key_Page_Down:
                return SDL_SCANCODE_PAGEDOWN;
            case Key_Home:
                return SDL_SCANCODE_HOME;
            case Key_End:
                return SDL_SCANCODE_END;
            case Key_Caps_Lock:
                return SDL_SCANCODE_CAPSLOCK;
            case Key_Scroll_Lock:
                return SDL_SCANCODE_SCROLLLOCK;
            case Key_Num_Lock:
                return SDL_SCANCODE_NUMLOCKCLEAR;
            case Key_Print_Screen:
                return SDL_SCANCODE_PRINTSCREEN;
            case Key_Pause:
                return SDL_SCANCODE_PAUSE;
            case Key_F1:
                return SDL_SCANCODE_F1;
            case Key_F2:
                return SDL_SCANCODE_F2;
            case Key_F3:
                return SDL_SCANCODE_F3;
            case Key_F4:
                return SDL_SCANCODE_F4;
            case Key_F5:
                return SDL_SCANCODE_F5;
            case Key_F6:
                return SDL_SCANCODE_F6;
            case Key_F7:
                return SDL_SCANCODE_F7;
            case Key_F8:
                return SDL_SCANCODE_F8;
            case Key_F9:
                return SDL_SCANCODE_F9;
            case Key_F10:
                return SDL_SCANCODE_F10;
            case Key_F11:
                return SDL_SCANCODE_F11;
            case Key_F12:
                return SDL_SCANCODE_F12;
            case Key_F13:
                return SDL_SCANCODE_F13;
            case Key_F14:
                return SDL_SCANCODE_F14;
            case Key_F15:
                return SDL_SCANCODE_F15;
            case Key_F16:
                return SDL_SCANCODE_F16;
            case Key_F17:
                return SDL_SCANCODE_F17;
            case Key_F18:
                return SDL_SCANCODE_F18;
            case Key_F19:
                return SDL_SCANCODE_F19;
            case Key_F20:
                return SDL_SCANCODE_F20;
            case Key_F21:
                return SDL_SCANCODE_F21;
            case Key_F22:
                return SDL_SCANCODE_F22;
            case Key_F23:
                return SDL_SCANCODE_F23;
            case Key_F24:
                return SDL_SCANCODE_F24;
            case Key_KP_0:
                return SDL_SCANCODE_KP_0;
            case Key_KP_1:
                return SDL_SCANCODE_KP_1;
            case Key_KP_2:
                return SDL_SCANCODE_KP_2;
            case Key_KP_3:
                return SDL_SCANCODE_KP_3;
            case Key_KP_4:
                return SDL_SCANCODE_KP_4;
            case Key_KP_5:
                return SDL_SCANCODE_KP_5;
            case Key_KP_6:
                return SDL_SCANCODE_KP_6;
            case Key_KP_7:
                return SDL_SCANCODE_KP_7;
            case Key_KP_8:
                return SDL_SCANCODE_KP_8;
            case Key_KP_9:
                return SDL_SCANCODE_KP_9;
            case Key_KP_Decimal:
                return SDL_SCANCODE_KP_PERIOD;
            case Key_KP_Divide:
                return SDL_SCANCODE_KP_DIVIDE;
            case Key_KP_Multiply:
                return SDL_SCANCODE_KP_MULTIPLY;
            case Key_KP_Subtract:
                return SDL_SCANCODE_KP_MINUS;
            case Key_KP_Add:
                return SDL_SCANCODE_KP_PLUS;
            case Key_KP_Enter:
                return SDL_SCANCODE_KP_ENTER;
            case Key_KP_Equal:
                return SDL_SCANCODE_KP_EQUALS;
            case Key_Left_Shift:
                return SDL_SCANCODE_LSHIFT;
            case Key_Left_Control:
                return SDL_SCANCODE_LCTRL;
            case Key_Left_Alt:
                return SDL_SCANCODE_LALT;
            case Key_Left_Super:
                return SDL_SCANCODE_LGUI;
            case Key_Right_Shift:
                return SDL_SCANCODE_RSHIFT;
            case Key_Right_Control:
                return SDL_SCANCODE_RCTRL;
            case Key_Right_Alt:
                return SDL_SCANCODE_RALT;
            case Key_Right_Super:
                return SDL_SCANCODE_RGUI;
            case Key_Menu:
                return SDL_SCANCODE_APPLICATION;
            default:
                return SDL_SCANCODE_UNKNOWN;
        }
    }

    auto FromSDLScancode( SDL_Scancode key ) -> KeyCode {
        MKT_BEGIN_PROFILER();

        switch ( key ) {
            case SDL_SCANCODE_SPACE:
                return Key_Space;
            case SDL_SCANCODE_APOSTROPHE:
                return Key_Apostrophe;
            case SDL_SCANCODE_COMMA:
                return Key_Comma;
            case SDL_SCANCODE_MINUS:
                return Key_Minus;
            case SDL_SCANCODE_PERIOD:
                return Key_Period;
            case SDL_SCANCODE_SLASH:
                return Key_Slash;
            case SDL_SCANCODE_0:
                return Key_0;
            case SDL_SCANCODE_1:
                return Key_1;
            case SDL_SCANCODE_2:
                return Key_2;
            case SDL_SCANCODE_3:
                return Key_3;
            case SDL_SCANCODE_4:
                return Key_4;
            case SDL_SCANCODE_5:
                return Key_5;
            case SDL_SCANCODE_6:
                return Key_6;
            case SDL_SCANCODE_7:
                return Key_7;
            case SDL_SCANCODE_8:
                return Key_8;
            case SDL_SCANCODE_9:
                return Key_9;
            case SDL_SCANCODE_SEMICOLON:
                return Key_Semicolon;
            case SDL_SCANCODE_EQUALS:
                return Key_Equal;
            case SDL_SCANCODE_A:
                return Key_A;
            case SDL_SCANCODE_B:
                return Key_B;
            case SDL_SCANCODE_C:
                return Key_C;
            case SDL_SCANCODE_D:
                return Key_D;
            case SDL_SCANCODE_E:
                return Key_E;
            case SDL_SCANCODE_F:
                return Key_F;
            case SDL_SCANCODE_G:
                return Key_G;
            case SDL_SCANCODE_H:
                return Key_H;
            case SDL_SCANCODE_I:
                return Key_I;
            case SDL_SCANCODE_J:
                return Key_J;
            case SDL_SCANCODE_K:
                return Key_K;
            case SDL_SCANCODE_L:
                return Key_L;
            case SDL_SCANCODE_M:
                return Key_M;
            case SDL_SCANCODE_N:
                return Key_N;
            case SDL_SCANCODE_O:
                return Key_O;
            case SDL_SCANCODE_P:
                return Key_P;
            case SDL_SCANCODE_Q:
                return Key_Q;
            case SDL_SCANCODE_R:
                return Key_R;
            case SDL_SCANCODE_S:
                return Key_S;
            case SDL_SCANCODE_T:
                return Key_T;
            case SDL_SCANCODE_U:
                return Key_U;
            case SDL_SCANCODE_V:
                return Key_V;
            case SDL_SCANCODE_W:
                return Key_W;
            case SDL_SCANCODE_X:
                return Key_X;
            case SDL_SCANCODE_Y:
                return Key_Y;
            case SDL_SCANCODE_Z:
                return Key_Z;
            case SDL_SCANCODE_LEFTBRACKET:
                return Key_Left_Bracket;
            case SDL_SCANCODE_BACKSLASH:
                return Key_Backslash;
            case SDL_SCANCODE_RIGHTBRACKET:
                return Key_Right_Bracket;
            case SDL_SCANCODE_GRAVE:
                return Key_Grave_Accent;
            case SDL_SCANCODE_INTERNATIONAL1:
                return Key_World_1;
            case SDL_SCANCODE_NONUSBACKSLASH:
                return Key_World_2;
            case SDL_SCANCODE_ESCAPE:
                return Key_Escape;
            case SDL_SCANCODE_RETURN:
                return Key_Enter;
            case SDL_SCANCODE_TAB:
                return Key_Tab;
            case SDL_SCANCODE_BACKSPACE:
                return Key_Backspace;
            case SDL_SCANCODE_INSERT:
                return Key_Insert;
            case SDL_SCANCODE_DELETE:
                return Key_Delete;
            case SDL_SCANCODE_RIGHT:
                return Key_Right;
            case SDL_SCANCODE_LEFT:
                return Key_Left;
            case SDL_SCANCODE_DOWN:
                return Key_Down;
            case SDL_SCANCODE_UP:
                return Key_Up;
            case SDL_SCANCODE_PAGEUP:
                return Key_Page_Up;
            case SDL_SCANCODE_PAGEDOWN:
                return Key_Page_Down;
            case SDL_SCANCODE_HOME:
                return Key_Home;
            case SDL_SCANCODE_END:
                return Key_End;
            case SDL_SCANCODE_CAPSLOCK:
                return Key_Caps_Lock;
            case SDL_SCANCODE_SCROLLLOCK:
                return Key_Scroll_Lock;
            case SDL_SCANCODE_NUMLOCKCLEAR:
                return Key_Num_Lock;
            case SDL_SCANCODE_PRINTSCREEN:
                return Key_Print_Screen;
            case SDL_SCANCODE_PAUSE:
                return Key_Pause;
            case SDL_SCANCODE_F1:
                return Key_F1;
            case SDL_SCANCODE_F2:
                return Key_F2;
            case SDL_SCANCODE_F3:
                return Key_F3;
            case SDL_SCANCODE_F4:
                return Key_F4;
            case SDL_SCANCODE_F5:
                return Key_F5;
            case SDL_SCANCODE_F6:
                return Key_F6;
            case SDL_SCANCODE_F7:
                return Key_F7;
            case SDL_SCANCODE_F8:
                return Key_F8;
            case SDL_SCANCODE_F9:
                return Key_F9;
            case SDL_SCANCODE_F10:
                return Key_F10;
            case SDL_SCANCODE_F11:
                return Key_F11;
            case SDL_SCANCODE_F12:
                return Key_F12;
            case SDL_SCANCODE_F13:
                return Key_F13;
            case SDL_SCANCODE_F14:
                return Key_F14;
            case SDL_SCANCODE_F15:
                return Key_F15;
            case SDL_SCANCODE_F16:
                return Key_F16;
            case SDL_SCANCODE_F17:
                return Key_F17;
            case SDL_SCANCODE_F18:
                return Key_F18;
            case SDL_SCANCODE_F19:
                return Key_F19;
            case SDL_SCANCODE_F20:
                return Key_F20;
            case SDL_SCANCODE_F21:
                return Key_F21;
            case SDL_SCANCODE_F22:
                return Key_F22;
            case SDL_SCANCODE_F23:
                return Key_F23;
            case SDL_SCANCODE_F24:
                return Key_F24;
            case SDL_SCANCODE_KP_0:
                return Key_KP_0;
            case SDL_SCANCODE_KP_1:
                return Key_KP_1;
            case SDL_SCANCODE_KP_2:
                return Key_KP_2;
            case SDL_SCANCODE_KP_3:
                return Key_KP_3;
            case SDL_SCANCODE_KP_4:
                return Key_KP_4;
            case SDL_SCANCODE_KP_5:
                return Key_KP_5;
            case SDL_SCANCODE_KP_6:
                return Key_KP_6;
            case SDL_SCANCODE_KP_7:
                return Key_KP_7;
            case SDL_SCANCODE_KP_8:
                return Key_KP_8;
            case SDL_SCANCODE_KP_9:
                return Key_KP_9;
            case SDL_SCANCODE_KP_PERIOD:
                return Key_KP_Decimal;
            case SDL_SCANCODE_KP_DIVIDE:
                return Key_KP_Divide;
            case SDL_SCANCODE_KP_MULTIPLY:
                return Key_KP_Multiply;
            case SDL_SCANCODE_KP_MINUS:
                return Key_KP_Subtract;
            case SDL_SCANCODE_KP_PLUS:
                return Key_KP_Add;
            case SDL_SCANCODE_KP_ENTER:
                return Key_KP_Enter;
            case SDL_SCANCODE_KP_EQUALS:
                return Key_KP_Equal;
            case SDL_SCANCODE_LSHIFT:
                return Key_Left_Shift;
            case SDL_SCANCODE_LCTRL:
                return Key_Left_Control;
            case SDL_SCANCODE_LALT:
                return Key_Left_Alt;
            case SDL_SCANCODE_LGUI:
                return Key_Left_Super;
            case SDL_SCANCODE_RSHIFT:
                return Key_Right_Shift;
            case SDL_SCANCODE_RCTRL:
                return Key_Right_Control;
            case SDL_SCANCODE_RALT:
                return Key_Right_Alt;
            case SDL_SCANCODE_RGUI:
                return Key_Right_Super;
            case SDL_SCANCODE_APPLICATION:
                return Key_Menu;
            default:
                return Key_Unknown;
        }
    }

    auto FromSDLModifiers( SDL_Keymod modifiers ) -> ModKey {
        MKT_BEGIN_PROFILER();
        ModKey result{ ModKey::eNone };

        if ( modifiers & SDL_KMOD_SHIFT ) {
            result = result | ModKey::eShift;
        }

        if ( modifiers & SDL_KMOD_CTRL ) {
            result = result | ModKey::eControl;
        }

        if ( modifiers & SDL_KMOD_ALT ) {
            result = result | ModKey::eAlt;
        }

        if ( modifiers & SDL_KMOD_GUI ) {
            result = result | ModKey::eSuper;
        }

        if ( modifiers & SDL_KMOD_CAPS ) {
            result = result | ModKey::eCapsLock;
        }

        if ( modifiers & SDL_KMOD_NUM ) {
            result = result | ModKey::eNumLock;
        }

        return result;
    }

    auto FromSDLMouseButton( core::u8 button ) -> MouseButton {
        MKT_BEGIN_PROFILER();

        switch ( button ) {
            case SDL_BUTTON_LEFT:
                return Mouse_Button_Left;
            case SDL_BUTTON_RIGHT:
                return Mouse_Button_Right;
            case SDL_BUTTON_MIDDLE:
                return Mouse_Button_Middle;
            case SDL_BUTTON_X1:
                return Mouse_Button_4;
            case SDL_BUTTON_X2:
                return Mouse_Button_5;
            case 6:
                return Mouse_Button_6;
            case 7:
                return Mouse_Button_7;
            case 8:
                return Mouse_Button_8;
            default:
                return Mouse_Button_Unknown;
        }
    }

    auto ConvertToSDLMouseButton( MouseButton button ) -> core::u32 {
        MKT_BEGIN_PROFILER();

        switch ( button ) {
            case Mouse_Button_Left:
                return SDL_BUTTON_LMASK;
            case Mouse_Button_Right:
                return SDL_BUTTON_RMASK;
            case Mouse_Button_Middle:
                return SDL_BUTTON_MMASK;
            case Mouse_Button_4:
                return SDL_BUTTON_X1MASK;
            case Mouse_Button_5:
                return SDL_BUTTON_X2MASK;
            case Mouse_Button_6:
                return SDL_BUTTON_MASK( 6 );
            case Mouse_Button_7:
                return SDL_BUTTON_MASK( 7 );
            case Mouse_Button_8:
                return SDL_BUTTON_MASK( 8 );
            default:
                return 0;
        }
    }
#endif

}// namespace mikoto::platform
