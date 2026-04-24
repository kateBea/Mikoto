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

#include <Core/InputSystem.hh>
#include <Scripting/InputBinding.hh>

namespace mikoto::scripting {

    using namespace mikoto::core;

    auto InputBinding::Init( sol::state &state ) -> void {
        constexpr bool isReadOnly{ true };

        const std::initializer_list<std::pair<sol::string_view, KeyCode>> keys{
            { "Space", KeyCode::Key_Space },
            { "Apostrophe", KeyCode::Key_Apostrophe },
            { "Comma", KeyCode::Key_Comma },
            { "Minus", KeyCode::Key_Minus },
            { "Period", KeyCode::Key_Period },
            { "Slash", KeyCode::Key_Slash },

            { "Num0", KeyCode::Key_0 },
            { "Num1", KeyCode::Key_1 },
            { "Num2", KeyCode::Key_2 },
            { "Num3", KeyCode::Key_3 },
            { "Num4", KeyCode::Key_4 },
            { "Num5", KeyCode::Key_5 },
            { "Num6", KeyCode::Key_6 },
            { "Num7", KeyCode::Key_7 },
            { "Num8", KeyCode::Key_8 },
            { "Num9", KeyCode::Key_9 },

            { "Semicolon", KeyCode::Key_Semicolon },
            { "Equal", KeyCode::Key_Equal },

            { "A", KeyCode::Key_A },
            { "B", KeyCode::Key_B },
            { "C", KeyCode::Key_C },
            { "D", KeyCode::Key_D },
            { "E", KeyCode::Key_E },
            { "F", KeyCode::Key_F },
            { "G", KeyCode::Key_G },
            { "H", KeyCode::Key_H },
            { "I", KeyCode::Key_I },
            { "J", KeyCode::Key_J },
            { "K", KeyCode::Key_K },
            { "L", KeyCode::Key_L },
            { "M", KeyCode::Key_M },
            { "N", KeyCode::Key_N },
            { "O", KeyCode::Key_O },
            { "P", KeyCode::Key_P },
            { "Q", KeyCode::Key_Q },
            { "R", KeyCode::Key_R },
            { "S", KeyCode::Key_S },
            { "T", KeyCode::Key_T },
            { "U", KeyCode::Key_U },
            { "V", KeyCode::Key_V },
            { "W", KeyCode::Key_W },
            { "X", KeyCode::Key_X },
            { "Y", KeyCode::Key_Y },
            { "Z", KeyCode::Key_Z },

            { "LeftBracket", KeyCode::Key_Left_Bracket },
            { "Backslash", KeyCode::Key_Backslash },
            { "RightBracket", KeyCode::Key_Right_Bracket },
            { "GraveAccent", KeyCode::Key_Grave_Accent },
            { "World1", KeyCode::Key_World_1 },
            { "World2", KeyCode::Key_World_2 },

            { "Escape", KeyCode::Key_Escape },
            { "Enter", KeyCode::Key_Enter },
            { "Tab", KeyCode::Key_Tab },
            { "Backspace", KeyCode::Key_Backspace },
            { "Insert", KeyCode::Key_Insert },
            { "Delete", KeyCode::Key_Delete },
            { "Right", KeyCode::Key_Right },
            { "Left", KeyCode::Key_Left },
            { "Down", KeyCode::Key_Down },
            { "Up", KeyCode::Key_Up },
            { "PageUp", KeyCode::Key_Page_Up },
            { "PageDown", KeyCode::Key_Page_Down },
            { "Home", KeyCode::Key_Home },
            { "End", KeyCode::Key_End },
            { "CapsLock", KeyCode::Key_Caps_Lock },
            { "ScrollLock", KeyCode::Key_Scroll_Lock },
            { "NumLock", KeyCode::Key_Num_Lock },
            { "PrintScreen", KeyCode::Key_Print_Screen },
            { "Pause", KeyCode::Key_Pause },

            { "F1", KeyCode::Key_F1 },
            { "F2", KeyCode::Key_F2 },
            { "F3", KeyCode::Key_F3 },
            { "F4", KeyCode::Key_F4 },
            { "F5", KeyCode::Key_F5 },
            { "F6", KeyCode::Key_F6 },
            { "F7", KeyCode::Key_F7 },
            { "F8", KeyCode::Key_F8 },
            { "F9", KeyCode::Key_F9 },
            { "F10", KeyCode::Key_F10 },
            { "F11", KeyCode::Key_F11 },
            { "F12", KeyCode::Key_F12 },
            { "F13", KeyCode::Key_F13 },
            { "F14", KeyCode::Key_F14 },
            { "F15", KeyCode::Key_F15 },
            { "F16", KeyCode::Key_F16 },
            { "F17", KeyCode::Key_F17 },
            { "F18", KeyCode::Key_F18 },
            { "F19", KeyCode::Key_F19 },
            { "F20", KeyCode::Key_F20 },
            { "F21", KeyCode::Key_F21 },
            { "F22", KeyCode::Key_F22 },
            { "F23", KeyCode::Key_F23 },
            { "F24", KeyCode::Key_F24 },
            { "F25", KeyCode::Key_F25 },

            { "KP0", KeyCode::Key_KP_0 },
            { "KP1", KeyCode::Key_KP_1 },
            { "KP2", KeyCode::Key_KP_2 },
            { "KP3", KeyCode::Key_KP_3 },
            { "KP4", KeyCode::Key_KP_4 },
            { "KP5", KeyCode::Key_KP_5 },
            { "KP6", KeyCode::Key_KP_6 },
            { "KP7", KeyCode::Key_KP_7 },
            { "KP8", KeyCode::Key_KP_8 },
            { "KP9", KeyCode::Key_KP_9 },
            { "KPDecimal", KeyCode::Key_KP_Decimal },
            { "KPDivide", KeyCode::Key_KP_Divide },
            { "KPMultiply", KeyCode::Key_KP_Multiply },
            { "KPSubtract", KeyCode::Key_KP_Subtract },
            { "KPAdd", KeyCode::Key_KP_Add },
            { "KPEnter", KeyCode::Key_KP_Enter },
            { "KPEqual", KeyCode::Key_KP_Equal },

            { "LeftShift", KeyCode::Key_Left_Shift },
            { "LeftControl", KeyCode::Key_Left_Control },
            { "LeftAlt", KeyCode::Key_Left_Alt },
            { "LeftSuper", KeyCode::Key_Left_Super },
            { "RightShift", KeyCode::Key_Right_Shift },
            { "RightControl", KeyCode::Key_Right_Control },
            { "RightAlt", KeyCode::Key_Right_Alt },
            { "RightSuper", KeyCode::Key_Right_Super },
            { "Menu", KeyCode::Key_Menu }
        };

        state.new_enum<KeyCode, isReadOnly>( "KeyCode", keys );

        sol::table input{ state.create_named_table("Input")};
        input.set_function( "IsKeyPressed",
            []( const KeyCode key ) {
                return InputSystem::Get()->IsKeyPressed( key );
            } );
    }
}// namespace Mikoto
