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

#ifndef MIKOTO_PLATFORM_KEY_CODES_HH
#define MIKOTO_PLATFORM_KEY_CODES_HH

#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

// Stable Mikoto input codes. Backends translate native keys into these values.
#define MKT_KEY_SPACE         32
#define MKT_KEY_APOSTROPHE    39 /* ' */
#define MKT_KEY_COMMA         44 /* , */
#define MKT_KEY_MINUS         45 /* - */
#define MKT_KEY_PERIOD        46 /* . */
#define MKT_KEY_SLASH         47 /* / */
#define MKT_KEY_0             48
#define MKT_KEY_1             49
#define MKT_KEY_2             50
#define MKT_KEY_3             51
#define MKT_KEY_4             52
#define MKT_KEY_5             53
#define MKT_KEY_6             54
#define MKT_KEY_7             55
#define MKT_KEY_8             56
#define MKT_KEY_9             57
#define MKT_KEY_SEMICOLON     59 /* ; */
#define MKT_KEY_EQUAL         61 /* = */
#define MKT_KEY_A             65
#define MKT_KEY_B             66
#define MKT_KEY_C             67
#define MKT_KEY_D             68
#define MKT_KEY_E             69
#define MKT_KEY_F             70
#define MKT_KEY_G             71
#define MKT_KEY_H             72
#define MKT_KEY_I             73
#define MKT_KEY_J             74
#define MKT_KEY_K             75
#define MKT_KEY_L             76
#define MKT_KEY_M             77
#define MKT_KEY_N             78
#define MKT_KEY_O             79
#define MKT_KEY_P             80
#define MKT_KEY_Q             81
#define MKT_KEY_R             82
#define MKT_KEY_S             83
#define MKT_KEY_T             84
#define MKT_KEY_U             85
#define MKT_KEY_V             86
#define MKT_KEY_W             87
#define MKT_KEY_X             88
#define MKT_KEY_Y             89
#define MKT_KEY_Z             90
#define MKT_KEY_LEFT_BRACKET  91  /* [ */
#define MKT_KEY_BACKSLASH     92  /* \ */
#define MKT_KEY_RIGHT_BRACKET 93  /* ] */
#define MKT_KEY_GRAVE_ACCENT  96  /* ` */
#define MKT_KEY_WORLD_1       161 /* non-US #1 */
#define MKT_KEY_WORLD_2       162 /* non-US #2 */

/* Function keys */
#define MKT_KEY_ESCAPE                    256
#define MKT_KEY_ENTER                     257
#define MKT_KEY_TAB                       258
#define MKT_KEY_BACKSPACE                 259
#define MKT_KEY_INSERT                    260
#define MKT_KEY_DELETE                    261
#define MKT_KEY_RIGHT                     262
#define MKT_KEY_LEFT                      263
#define MKT_KEY_DOWN                      264
#define MKT_KEY_UP                        265
#define MKT_KEY_PAGE_UP                   266
#define MKT_KEY_PAGE_DOWN                 267
#define MKT_KEY_HOME                      268
#define MKT_KEY_END                       269
#define MKT_KEY_CAPS_LOCK                 280
#define MKT_KEY_SCROLL_LOCK               281
#define MKT_KEY_NUM_LOCK                  282
#define MKT_KEY_PRINT_SCREEN              283
#define MKT_KEY_PAUSE                     284
#define MKT_KEY_F1                        290
#define MKT_KEY_F2                        291
#define MKT_KEY_F3                        292
#define MKT_KEY_F4                        293
#define MKT_KEY_F5                        294
#define MKT_KEY_F6                        295
#define MKT_KEY_F7                        296
#define MKT_KEY_F8                        297
#define MKT_KEY_F9                        298
#define MKT_KEY_F10                       299
#define MKT_KEY_F11                       300
#define MKT_KEY_F12                       301
#define MKT_KEY_F13                       302
#define MKT_KEY_F14                       303
#define MKT_KEY_F15                       304
#define MKT_KEY_F16                       305
#define MKT_KEY_F17                       306
#define MKT_KEY_F18                       307
#define MKT_KEY_F19                       308
#define MKT_KEY_F20                       309
#define MKT_KEY_F21                       310
#define MKT_KEY_F22                       311
#define MKT_KEY_F23                       312
#define MKT_KEY_F24                       313
#define MKT_KEY_F25                       314
#define MKT_KEY_KP_0                      320
#define MKT_KEY_KP_1                      321
#define MKT_KEY_KP_2                      322
#define MKT_KEY_KP_3                      323
#define MKT_KEY_KP_4                      324
#define MKT_KEY_KP_5                      325
#define MKT_KEY_KP_6                      326
#define MKT_KEY_KP_7                      327
#define MKT_KEY_KP_8                      328
#define MKT_KEY_KP_9                      329
#define MKT_KEY_KP_DECIMAL                330
#define MKT_KEY_KP_DIVIDE                 331
#define MKT_KEY_KP_MULTIPLY               332
#define MKT_KEY_KP_SUBTRACT               333
#define MKT_KEY_KP_ADD                    334
#define MKT_KEY_KP_ENTER                  335
#define MKT_KEY_KP_EQUAL                  336
#define MKT_KEY_LEFT_SHIFT                340
#define MKT_KEY_LEFT_CONTROL              341
#define MKT_KEY_LEFT_ALT                  342
#define MKT_KEY_LEFT_SUPER                343
#define MKT_KEY_RIGHT_SHIFT               344
#define MKT_KEY_RIGHT_CONTROL             345
#define MKT_KEY_RIGHT_ALT                 346
#define MKT_KEY_RIGHT_SUPER               347
#define MKT_KEY_MENU                      348
#define MKT_MOD_SHIFT                     0x0001
#define MKT_MOD_CONTROL                   0x0002
#define MKT_MOD_ALT                       0x0004
#define MKT_MOD_SUPER                     0x0008
#define MKT_MOD_CAPS_LOCK                 0x0010
#define MKT_MOD_NUM_LOCK                  0x0020

#define KEY_STRING_REPRESENTATION( name ) #name

namespace mikoto::platform {

    enum class ModKey : core::i32 {
        eNone = 0,
        eShift = MKT_MOD_SHIFT,
        eControl = MKT_MOD_CONTROL,
        eAlt = MKT_MOD_ALT,
        eSuper = MKT_MOD_SUPER,
        eCapsLock = MKT_MOD_CAPS_LOCK,
        eNumLock = MKT_MOD_NUM_LOCK
    };

    enum KeyCode : core::i16 {
        Key_Unknown = -1,
        Key_Space = MKT_KEY_SPACE,
        Key_Apostrophe = MKT_KEY_APOSTROPHE, /* ' */
        Key_Comma = MKT_KEY_COMMA,           /* , */
        Key_Minus = MKT_KEY_MINUS,           /* - */
        Key_Period = MKT_KEY_PERIOD,         /* . */
        Key_Slash = MKT_KEY_SLASH,           /* / */
        Key_0 = MKT_KEY_0,
        Key_1 = MKT_KEY_1,
        Key_2 = MKT_KEY_2,
        Key_3 = MKT_KEY_3,
        Key_4 = MKT_KEY_4,
        Key_5 = MKT_KEY_5,
        Key_6 = MKT_KEY_6,
        Key_7 = MKT_KEY_7,
        Key_8 = MKT_KEY_8,
        Key_9 = MKT_KEY_9,

        Key_Semicolon = MKT_KEY_SEMICOLON, /* ; */
        Key_Equal = MKT_KEY_EQUAL,         /* = */

        Key_A = MKT_KEY_A,
        Key_B = MKT_KEY_B,
        Key_C = MKT_KEY_C,
        Key_D = MKT_KEY_D,
        Key_E = MKT_KEY_E,
        Key_F = MKT_KEY_F,
        Key_G = MKT_KEY_G,
        Key_H = MKT_KEY_H,
        Key_I = MKT_KEY_I,
        Key_J = MKT_KEY_J,
        Key_K = MKT_KEY_K,
        Key_L = MKT_KEY_L,
        Key_M = MKT_KEY_M,
        Key_N = MKT_KEY_N,
        Key_O = MKT_KEY_O,
        Key_P = MKT_KEY_P,
        Key_Q = MKT_KEY_Q,
        Key_R = MKT_KEY_R,
        Key_S = MKT_KEY_S,
        Key_T = MKT_KEY_T,
        Key_U = MKT_KEY_U,
        Key_V = MKT_KEY_V,
        Key_W = MKT_KEY_W,
        Key_X = MKT_KEY_X,
        Key_Y = MKT_KEY_Y,
        Key_Z = MKT_KEY_Z,
        Key_Left_Bracket = MKT_KEY_LEFT_BRACKET,   /* [ */
        Key_Backslash = MKT_KEY_BACKSLASH,         /* \ */
        Key_Right_Bracket = MKT_KEY_RIGHT_BRACKET, /* ] */
        Key_Grave_Accent = MKT_KEY_GRAVE_ACCENT,   /* ` */
        Key_World_1 = MKT_KEY_WORLD_1,             /* non-US #1 */
        Key_World_2 = MKT_KEY_WORLD_2,             /* non-US #2 */

        Key_Escape = MKT_KEY_ESCAPE,
        Key_Enter = MKT_KEY_ENTER,
        Key_Tab = MKT_KEY_TAB,
        Key_Backspace = MKT_KEY_BACKSPACE,
        Key_Insert = MKT_KEY_INSERT,
        Key_Delete = MKT_KEY_DELETE,
        Key_Right = MKT_KEY_RIGHT,
        Key_Left = MKT_KEY_LEFT,
        Key_Down = MKT_KEY_DOWN,
        Key_Up = MKT_KEY_UP,
        Key_Page_Up = MKT_KEY_PAGE_UP,
        Key_Page_Down = MKT_KEY_PAGE_DOWN,
        Key_Home = MKT_KEY_HOME,
        Key_End = MKT_KEY_END,
        Key_Caps_Lock = MKT_KEY_CAPS_LOCK,
        Key_Scroll_Lock = MKT_KEY_SCROLL_LOCK,
        Key_Num_Lock = MKT_KEY_NUM_LOCK,
        Key_Print_Screen = MKT_KEY_PRINT_SCREEN,
        Key_Pause = MKT_KEY_PAUSE,

        Key_F1 = MKT_KEY_F1,
        Key_F2 = MKT_KEY_F2,
        Key_F3 = MKT_KEY_F3,
        Key_F4 = MKT_KEY_F4,
        Key_F5 = MKT_KEY_F5,
        Key_F6 = MKT_KEY_F6,
        Key_F7 = MKT_KEY_F7,
        Key_F8 = MKT_KEY_F8,
        Key_F9 = MKT_KEY_F9,
        Key_F10 = MKT_KEY_F10,
        Key_F11 = MKT_KEY_F11,
        Key_F12 = MKT_KEY_F12,
        Key_F13 = MKT_KEY_F13,
        Key_F14 = MKT_KEY_F14,
        Key_F15 = MKT_KEY_F15,
        Key_F16 = MKT_KEY_F16,
        Key_F17 = MKT_KEY_F17,
        Key_F18 = MKT_KEY_F18,
        Key_F19 = MKT_KEY_F19,
        Key_F20 = MKT_KEY_F20,
        Key_F21 = MKT_KEY_F21,
        Key_F22 = MKT_KEY_F22,
        Key_F23 = MKT_KEY_F23,
        Key_F24 = MKT_KEY_F24,
        Key_F25 = MKT_KEY_F25,

        Key_KP_0 = MKT_KEY_KP_0,
        Key_KP_1 = MKT_KEY_KP_1,
        Key_KP_2 = MKT_KEY_KP_2,
        Key_KP_3 = MKT_KEY_KP_3,
        Key_KP_4 = MKT_KEY_KP_4,
        Key_KP_5 = MKT_KEY_KP_5,
        Key_KP_6 = MKT_KEY_KP_6,
        Key_KP_7 = MKT_KEY_KP_7,
        Key_KP_8 = MKT_KEY_KP_8,
        Key_KP_9 = MKT_KEY_KP_9,
        Key_KP_Decimal = MKT_KEY_KP_DECIMAL,
        Key_KP_Divide = MKT_KEY_KP_DIVIDE,
        Key_KP_Multiply = MKT_KEY_KP_MULTIPLY,
        Key_KP_Subtract = MKT_KEY_KP_SUBTRACT,
        Key_KP_Add = MKT_KEY_KP_ADD,
        Key_KP_Enter = MKT_KEY_KP_ENTER,
        Key_KP_Equal = MKT_KEY_KP_EQUAL,

        Key_Left_Shift = MKT_KEY_LEFT_SHIFT,
        Key_Left_Control = MKT_KEY_LEFT_CONTROL,
        Key_Left_Alt = MKT_KEY_LEFT_ALT,
        Key_Left_Super = MKT_KEY_LEFT_SUPER,
        Key_Right_Shift = MKT_KEY_RIGHT_SHIFT,
        Key_Right_Control = MKT_KEY_RIGHT_CONTROL,
        Key_Right_Alt = MKT_KEY_RIGHT_ALT,
        Key_Right_Super = MKT_KEY_RIGHT_SUPER,
        Key_Menu = MKT_KEY_MENU,
    };

    MKT_NODISCARD auto operator|( ModKey a, ModKey b ) -> ModKey;
    MKT_NODISCARD auto operator&( ModKey a, ModKey b ) -> ModKey;

}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_KEY_CODES_HH
