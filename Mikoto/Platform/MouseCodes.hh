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

#ifndef MIKOTO_PLATFORM_MOUSE_CODES_HH
#define MIKOTO_PLATFORM_MOUSE_CODES_HH

#include <Core/Types.hh>

// Stable Mikoto mouse codes. Backends translate native buttons into these values.
#define MKT_MOUSE_BUTTON_1                         0
#define MKT_MOUSE_BUTTON_2                         1
#define MKT_MOUSE_BUTTON_3                         2
#define MKT_MOUSE_BUTTON_4                         3
#define MKT_MOUSE_BUTTON_5                         4
#define MKT_MOUSE_BUTTON_6                         5
#define MKT_MOUSE_BUTTON_7                         6
#define MKT_MOUSE_BUTTON_8                         7
#define MKT_MOUSE_BUTTON_LAST                      7
#define MKT_MOUSE_BUTTON_LEFT                      0
#define MKT_MOUSE_BUTTON_RIGHT                     1
#define MKT_MOUSE_BUTTON_MIDDLE                    2

#define MOUSE_BUTTON_STRING_REPRESENTATION( name ) #name

namespace mikoto::platform {

    enum MouseButton : core::i8 {
        Mouse_Button_Unknown = -1,
        Mouse_Button_1 = MKT_MOUSE_BUTTON_1,
        Mouse_Button_2 = MKT_MOUSE_BUTTON_2,
        Mouse_Button_3 = MKT_MOUSE_BUTTON_3,
        Mouse_Button_4 = MKT_MOUSE_BUTTON_4,
        Mouse_Button_5 = MKT_MOUSE_BUTTON_5,
        Mouse_Button_6 = MKT_MOUSE_BUTTON_6,
        Mouse_Button_7 = MKT_MOUSE_BUTTON_7,
        Mouse_Button_8 = MKT_MOUSE_BUTTON_8,

        Mouse_Button_Last = MKT_MOUSE_BUTTON_LAST,
        Mouse_Button_Left = MKT_MOUSE_BUTTON_LEFT,
        Mouse_Button_Right = MKT_MOUSE_BUTTON_RIGHT,
        Mouse_Button_Middle = MKT_MOUSE_BUTTON_MIDDLE,
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_MOUSE_CODES_HH
