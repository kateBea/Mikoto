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


#include <initializer_list>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Platform/InputSystem.hh>

#include <Scripting/Lua/LuaBindings.hh>
#include <Scripting/ScriptingService.hh>


namespace mikoto::scripting {

    using namespace mikoto::core;

    auto InputBinding::Init( sol::state &state ) -> void {
        MKT_BEGIN_PROFILER();
        constexpr bool isReadOnly{ true };

        const std::initializer_list<std::pair<sol::string_view, platform::KeyCode>> keys{
            { "Space", platform::KeyCode::Key_Space },
            { "Apostrophe", platform::KeyCode::Key_Apostrophe },
            { "Comma", platform::KeyCode::Key_Comma },
            { "Minus", platform::KeyCode::Key_Minus },
            { "Period", platform::KeyCode::Key_Period },
            { "Slash", platform::KeyCode::Key_Slash },

            { "Num0", platform::KeyCode::Key_0 },
            { "Num1", platform::KeyCode::Key_1 },
            { "Num2", platform::KeyCode::Key_2 },
            { "Num3", platform::KeyCode::Key_3 },
            { "Num4", platform::KeyCode::Key_4 },
            { "Num5", platform::KeyCode::Key_5 },
            { "Num6", platform::KeyCode::Key_6 },
            { "Num7", platform::KeyCode::Key_7 },
            { "Num8", platform::KeyCode::Key_8 },
            { "Num9", platform::KeyCode::Key_9 },

            { "Semicolon", platform::KeyCode::Key_Semicolon },
            { "Equal", platform::KeyCode::Key_Equal },

            { "A", platform::KeyCode::Key_A },
            { "B", platform::KeyCode::Key_B },
            { "C", platform::KeyCode::Key_C },
            { "D", platform::KeyCode::Key_D },
            { "E", platform::KeyCode::Key_E },
            { "F", platform::KeyCode::Key_F },
            { "G", platform::KeyCode::Key_G },
            { "H", platform::KeyCode::Key_H },
            { "I", platform::KeyCode::Key_I },
            { "J", platform::KeyCode::Key_J },
            { "K", platform::KeyCode::Key_K },
            { "L", platform::KeyCode::Key_L },
            { "M", platform::KeyCode::Key_M },
            { "N", platform::KeyCode::Key_N },
            { "O", platform::KeyCode::Key_O },
            { "P", platform::KeyCode::Key_P },
            { "Q", platform::KeyCode::Key_Q },
            { "R", platform::KeyCode::Key_R },
            { "S", platform::KeyCode::Key_S },
            { "T", platform::KeyCode::Key_T },
            { "U", platform::KeyCode::Key_U },
            { "V", platform::KeyCode::Key_V },
            { "W", platform::KeyCode::Key_W },
            { "X", platform::KeyCode::Key_X },
            { "Y", platform::KeyCode::Key_Y },
            { "Z", platform::KeyCode::Key_Z },

            { "LeftBracket", platform::KeyCode::Key_Left_Bracket },
            { "Backslash", platform::KeyCode::Key_Backslash },
            { "RightBracket", platform::KeyCode::Key_Right_Bracket },
            { "GraveAccent", platform::KeyCode::Key_Grave_Accent },
            { "World1", platform::KeyCode::Key_World_1 },
            { "World2", platform::KeyCode::Key_World_2 },

            { "Escape", platform::KeyCode::Key_Escape },
            { "Enter", platform::KeyCode::Key_Enter },
            { "Tab", platform::KeyCode::Key_Tab },
            { "Backspace", platform::KeyCode::Key_Backspace },
            { "Insert", platform::KeyCode::Key_Insert },
            { "Delete", platform::KeyCode::Key_Delete },
            { "Right", platform::KeyCode::Key_Right },
            { "Left", platform::KeyCode::Key_Left },
            { "Down", platform::KeyCode::Key_Down },
            { "Up", platform::KeyCode::Key_Up },
            { "PageUp", platform::KeyCode::Key_Page_Up },
            { "PageDown", platform::KeyCode::Key_Page_Down },
            { "Home", platform::KeyCode::Key_Home },
            { "End", platform::KeyCode::Key_End },
            { "CapsLock", platform::KeyCode::Key_Caps_Lock },
            { "ScrollLock", platform::KeyCode::Key_Scroll_Lock },
            { "NumLock", platform::KeyCode::Key_Num_Lock },
            { "PrintScreen", platform::KeyCode::Key_Print_Screen },
            { "Pause", platform::KeyCode::Key_Pause },

            { "F1", platform::KeyCode::Key_F1 },
            { "F2", platform::KeyCode::Key_F2 },
            { "F3", platform::KeyCode::Key_F3 },
            { "F4", platform::KeyCode::Key_F4 },
            { "F5", platform::KeyCode::Key_F5 },
            { "F6", platform::KeyCode::Key_F6 },
            { "F7", platform::KeyCode::Key_F7 },
            { "F8", platform::KeyCode::Key_F8 },
            { "F9", platform::KeyCode::Key_F9 },
            { "F10", platform::KeyCode::Key_F10 },
            { "F11", platform::KeyCode::Key_F11 },
            { "F12", platform::KeyCode::Key_F12 },
            { "F13", platform::KeyCode::Key_F13 },
            { "F14", platform::KeyCode::Key_F14 },
            { "F15", platform::KeyCode::Key_F15 },
            { "F16", platform::KeyCode::Key_F16 },
            { "F17", platform::KeyCode::Key_F17 },
            { "F18", platform::KeyCode::Key_F18 },
            { "F19", platform::KeyCode::Key_F19 },
            { "F20", platform::KeyCode::Key_F20 },
            { "F21", platform::KeyCode::Key_F21 },
            { "F22", platform::KeyCode::Key_F22 },
            { "F23", platform::KeyCode::Key_F23 },
            { "F24", platform::KeyCode::Key_F24 },
            { "F25", platform::KeyCode::Key_F25 },

            { "KP0", platform::KeyCode::Key_KP_0 },
            { "KP1", platform::KeyCode::Key_KP_1 },
            { "KP2", platform::KeyCode::Key_KP_2 },
            { "KP3", platform::KeyCode::Key_KP_3 },
            { "KP4", platform::KeyCode::Key_KP_4 },
            { "KP5", platform::KeyCode::Key_KP_5 },
            { "KP6", platform::KeyCode::Key_KP_6 },
            { "KP7", platform::KeyCode::Key_KP_7 },
            { "KP8", platform::KeyCode::Key_KP_8 },
            { "KP9", platform::KeyCode::Key_KP_9 },
            { "KPDecimal", platform::KeyCode::Key_KP_Decimal },
            { "KPDivide", platform::KeyCode::Key_KP_Divide },
            { "KPMultiply", platform::KeyCode::Key_KP_Multiply },
            { "KPSubtract", platform::KeyCode::Key_KP_Subtract },
            { "KPAdd", platform::KeyCode::Key_KP_Add },
            { "KPEnter", platform::KeyCode::Key_KP_Enter },
            { "KPEqual", platform::KeyCode::Key_KP_Equal },

            { "LeftShift", platform::KeyCode::Key_Left_Shift },
            { "LeftControl", platform::KeyCode::Key_Left_Control },
            { "LeftAlt", platform::KeyCode::Key_Left_Alt },
            { "LeftSuper", platform::KeyCode::Key_Left_Super },
            { "RightShift", platform::KeyCode::Key_Right_Shift },
            { "RightControl", platform::KeyCode::Key_Right_Control },
            { "RightAlt", platform::KeyCode::Key_Right_Alt },
            { "RightSuper", platform::KeyCode::Key_Right_Super },
            { "Menu", platform::KeyCode::Key_Menu }
        };

        state.new_enum<platform::KeyCode, isReadOnly>( "KeyCode", keys );

        sol::table input{ state.create_named_table( "Input" ) };
        input.set_function( "IsKeyPressed",
            []( const platform::KeyCode key ) {
            return ScriptingService::Get()->HasInputFocus() && platform::InputSystem::Get()->IsKeyPressed( key );
        } );
    }
}// namespace Mikoto
