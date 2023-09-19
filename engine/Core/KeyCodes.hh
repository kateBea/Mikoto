/**
 * KeyCodes.hh
 * Created by kate on 5/31/23.
 * */
#ifndef MIKOTO_KEYCODES_HH
#define MIKOTO_KEYCODES_HH

// C++ Standard Library
#include <string_view>

// Third-Party Libraries
#include <GLFW/glfw3.h>

// Project Headers
#include <Utility/Common.hh>

#if defined(USE_GLFW_KEY_CODES)
    #define MKT_KEY_SPACE              GLFW_KEY_SPACE
    #define MKT_KEY_APOSTROPHE         GLFW_KEY_APOSTROPHE   /* ' */
    #define MKT_KEY_COMMA              GLFW_KEY_COMMA        /* , */
    #define MKT_KEY_MINUS              GLFW_KEY_MINUS        /* - */
    #define MKT_KEY_PERIOD             GLFW_KEY_PERIOD       /* . */
    #define MKT_KEY_SLASH              GLFW_KEY_SLASH        /* / */
    #define MKT_KEY_0                  GLFW_KEY_0
    #define MKT_KEY_1                  GLFW_KEY_1
    #define MKT_KEY_2                  GLFW_KEY_2
    #define MKT_KEY_3                  GLFW_KEY_3
    #define MKT_KEY_4                  GLFW_KEY_4
    #define MKT_KEY_5                  GLFW_KEY_5
    #define MKT_KEY_6                  GLFW_KEY_6
    #define MKT_KEY_7                  GLFW_KEY_7
    #define MKT_KEY_8                  GLFW_KEY_8
    #define MKT_KEY_9                  GLFW_KEY_9
    #define MKT_KEY_SEMICOLON          GLFW_KEY_SEMICOLON    /* ; */
    #define MKT_KEY_EQUAL              GLFW_KEY_EQUAL        /* = */
    #define MKT_KEY_A                  GLFW_KEY_A
    #define MKT_KEY_B                  GLFW_KEY_B
    #define MKT_KEY_C                  GLFW_KEY_C
    #define MKT_KEY_D                  GLFW_KEY_D
    #define MKT_KEY_E                  GLFW_KEY_E
    #define MKT_KEY_F                  GLFW_KEY_F
    #define MKT_KEY_G                  GLFW_KEY_G
    #define MKT_KEY_H                  GLFW_KEY_H
    #define MKT_KEY_I                  GLFW_KEY_I
    #define MKT_KEY_J                  GLFW_KEY_J
    #define MKT_KEY_K                  GLFW_KEY_K
    #define MKT_KEY_L                  GLFW_KEY_L
    #define MKT_KEY_M                  GLFW_KEY_M
    #define MKT_KEY_N                  GLFW_KEY_N
    #define MKT_KEY_O                  GLFW_KEY_O
    #define MKT_KEY_P                  GLFW_KEY_P
    #define MKT_KEY_Q                  GLFW_KEY_Q
    #define MKT_KEY_R                  GLFW_KEY_R
    #define MKT_KEY_S                  GLFW_KEY_S
    #define MKT_KEY_T                  GLFW_KEY_T
    #define MKT_KEY_U                  GLFW_KEY_U
    #define MKT_KEY_V                  GLFW_KEY_V
    #define MKT_KEY_W                  GLFW_KEY_W
    #define MKT_KEY_X                  GLFW_KEY_X
    #define MKT_KEY_Y                  GLFW_KEY_Y
    #define MKT_KEY_Z                  GLFW_KEY_Z
    #define MKT_KEY_LEFT_BRACKET       GLFW_KEY_LEFT_BRACKET     /* [ */
    #define MKT_KEY_BACKSLASH          GLFW_KEY_BACKSLASH        /* \ */
    #define MKT_KEY_RIGHT_BRACKET      GLFW_KEY_RIGHT_BRACKET    /* ] */
    #define MKT_KEY_GRAVE_ACCENT       GLFW_KEY_GRAVE_ACCENT     /* ` */
    #define MKT_KEY_WORLD_1            GLFW_KEY_WORLD_1          /* non-US #1 */
    #define MKT_KEY_WORLD_2            GLFW_KEY_WORLD_2          /* non-US #2 */

    /* Function keys */
    #define MKT_KEY_ESCAPE             GLFW_KEY_ESCAPE
    #define MKT_KEY_ENTER              GLFW_KEY_ENTER
    #define MKT_KEY_TAB                GLFW_KEY_TAB
    #define MKT_KEY_BACKSPACE          GLFW_KEY_BACKSPACE
    #define MKT_KEY_INSERT             GLFW_KEY_INSERT
    #define MKT_KEY_DELETE             GLFW_KEY_DELETE
    #define MKT_KEY_RIGHT              GLFW_KEY_RIGHT
    #define MKT_KEY_LEFT               GLFW_KEY_LEFT
    #define MKT_KEY_DOWN               GLFW_KEY_DOWN
    #define MKT_KEY_UP                 GLFW_KEY_UP
    #define MKT_KEY_PAGE_UP            GLFW_KEY_PAGE_UP
    #define MKT_KEY_PAGE_DOWN          GLFW_KEY_PAGE_DOWN
    #define MKT_KEY_HOME               GLFW_KEY_HOME
    #define MKT_KEY_END                GLFW_KEY_END
    #define MKT_KEY_CAPS_LOCK          GLFW_KEY_CAPS_LOCK
    #define MKT_KEY_SCROLL_LOCK        GLFW_KEY_SCROLL_LOCK
    #define MKT_KEY_NUM_LOCK           GLFW_KEY_NUM_LOCK
    #define MKT_KEY_PRINT_SCREEN       GLFW_KEY_PRINT_SCREEN
    #define MKT_KEY_PAUSE              GLFW_KEY_PAUSE
    #define MKT_KEY_F1                 GLFW_KEY_F1
    #define MKT_KEY_F2                 GLFW_KEY_F2
    #define MKT_KEY_F3                 GLFW_KEY_F3
    #define MKT_KEY_F4                 GLFW_KEY_F4
    #define MKT_KEY_F5                 GLFW_KEY_F5
    #define MKT_KEY_F6                 GLFW_KEY_F6
    #define MKT_KEY_F7                 GLFW_KEY_F7
    #define MKT_KEY_F8                 GLFW_KEY_F8
    #define MKT_KEY_F9                 GLFW_KEY_F9
    #define MKT_KEY_F10                GLFW_KEY_F10
    #define MKT_KEY_F11                GLFW_KEY_F11
    #define MKT_KEY_F12                GLFW_KEY_F12
    #define MKT_KEY_F13                GLFW_KEY_F13
    #define MKT_KEY_F14                GLFW_KEY_F14
    #define MKT_KEY_F15                GLFW_KEY_F15
    #define MKT_KEY_F16                GLFW_KEY_F16
    #define MKT_KEY_F17                GLFW_KEY_F17
    #define MKT_KEY_F18                GLFW_KEY_F18
    #define MKT_KEY_F19                GLFW_KEY_F19
    #define MKT_KEY_F20                GLFW_KEY_F20
    #define MKT_KEY_F21                GLFW_KEY_F21
    #define MKT_KEY_F22                GLFW_KEY_F22
    #define MKT_KEY_F23                GLFW_KEY_F23
    #define MKT_KEY_F24                GLFW_KEY_F24
    #define MKT_KEY_F25                GLFW_KEY_F25
    #define MKT_KEY_KP_0               GLFW_KEY_KP_0
    #define MKT_KEY_KP_1               GLFW_KEY_KP_1
    #define MKT_KEY_KP_2               GLFW_KEY_KP_2
    #define MKT_KEY_KP_3               GLFW_KEY_KP_3
    #define MKT_KEY_KP_4               GLFW_KEY_KP_4
    #define MKT_KEY_KP_5               GLFW_KEY_KP_5
    #define MKT_KEY_KP_6               GLFW_KEY_KP_6
    #define MKT_KEY_KP_7               GLFW_KEY_KP_7
    #define MKT_KEY_KP_8               GLFW_KEY_KP_8
    #define MKT_KEY_KP_9               GLFW_KEY_KP_9
    #define MKT_KEY_KP_DECIMAL         GLFW_KEY_KP_DECIMAL
    #define MKT_KEY_KP_DIVIDE          GLFW_KEY_KP_DIVIDE
    #define MKT_KEY_KP_MULTIPLY        GLFW_KEY_KP_MULTIPLY
    #define MKT_KEY_KP_SUBTRACT        GLFW_KEY_KP_SUBTRACT
    #define MKT_KEY_KP_ADD             GLFW_KEY_KP_ADD
    #define MKT_KEY_KP_ENTER           GLFW_KEY_KP_ENTER
    #define MKT_KEY_KP_EQUAL           GLFW_KEY_KP_EQUAL
    #define MKT_KEY_LEFT_SHIFT         GLFW_KEY_LEFT_SHIFT
    #define MKT_KEY_LEFT_CONTROL       GLFW_KEY_LEFT_CONTROL
    #define MKT_KEY_LEFT_ALT           GLFW_KEY_LEFT_ALT
    #define MKT_KEY_LEFT_SUPER         GLFW_KEY_LEFT_SUPER
    #define MKT_KEY_RIGHT_SHIFT        GLFW_KEY_RIGHT_SHIFT
    #define MKT_KEY_RIGHT_CONTROL      GLFW_KEY_RIGHT_CONTROL
    #define MKT_KEY_RIGHT_ALT          GLFW_KEY_RIGHT_ALT
    #define MKT_KEY_RIGHT_SUPER        GLFW_KEY_RIGHT_SUPER
    #define MKT_KEY_MENU               GLFW_KEY_MENU
#endif

#define KEY_STRING_REPRESENTATION(name) #name



namespace Mikoto {
    enum KeyCode : Int16_T {
        Key_Space           = MKT_KEY_SPACE           ,
        Key_Apostrophe      = MKT_KEY_APOSTROPHE      , /* ' */
        Key_Comma           = MKT_KEY_COMMA           , /* , */
        Key_Minus           = MKT_KEY_MINUS           , /* - */
        Key_Period          = MKT_KEY_PERIOD          , /* . */
        Key_Slash           = MKT_KEY_SLASH           , /* / */
        Key_0               = MKT_KEY_0               ,
        Key_1               = MKT_KEY_1               ,
        Key_2               = MKT_KEY_2               ,
        Key_3               = MKT_KEY_3               ,
        Key_4               = MKT_KEY_4               ,
        Key_5               = MKT_KEY_5               ,
        Key_6               = MKT_KEY_6               ,
        Key_7               = MKT_KEY_7               ,
        Key_8               = MKT_KEY_8               ,
        Key_9               = MKT_KEY_9               ,

        Key_Semicolon       = MKT_KEY_SEMICOLON       , /* ; */
        Key_Equal           = MKT_KEY_EQUAL           , /* = */

        Key_A               = MKT_KEY_A               ,
        Key_B               = MKT_KEY_B               ,
        Key_C               = MKT_KEY_C               ,
        Key_D               = MKT_KEY_D               ,
        Key_E               = MKT_KEY_E               ,
        Key_F               = MKT_KEY_F               ,
        Key_G               = MKT_KEY_G               ,
        Key_H               = MKT_KEY_H               ,
        Key_I               = MKT_KEY_I               ,
        Key_J               = MKT_KEY_J               ,
        Key_K               = MKT_KEY_K               ,
        Key_L               = MKT_KEY_L               ,
        Key_M               = MKT_KEY_M               ,
        Key_N               = MKT_KEY_N               ,
        Key_O               = MKT_KEY_O               ,
        Key_P               = MKT_KEY_P               ,
        Key_Q               = MKT_KEY_Q               ,
        Key_R               = MKT_KEY_R               ,
        Key_S               = MKT_KEY_S               ,
        Key_T               = MKT_KEY_T               ,
        Key_U               = MKT_KEY_U               ,
        Key_V               = MKT_KEY_V               ,
        Key_W               = MKT_KEY_W               ,
        Key_X               = MKT_KEY_X               ,
        Key_Y               = MKT_KEY_Y               ,
        Key_Z               = MKT_KEY_Z               ,
        Key_Left_Bracket    = MKT_KEY_LEFT_BRACKET    ,    /* [ */
        Key_Backslash       = MKT_KEY_BACKSLASH       ,    /* \ */
        Key_Right_Bracket   = MKT_KEY_RIGHT_BRACKET   ,    /* ] */
        Key_Grave_Accent    = MKT_KEY_GRAVE_ACCENT    ,    /* ` */
        Key_World_1         = MKT_KEY_WORLD_1         ,    /* non-US #1 */
        Key_World_2         = MKT_KEY_WORLD_2         ,    /* non-US #2 */

        Key_Escape          = MKT_KEY_ESCAPE          ,
        Key_Enter           = MKT_KEY_ENTER           ,
        Key_Tab             = MKT_KEY_TAB             ,
        Key_Backspace       = MKT_KEY_BACKSPACE       ,
        Key_Insert          = MKT_KEY_INSERT          ,
        Key_Delete          = MKT_KEY_DELETE          ,
        Key_Right           = MKT_KEY_RIGHT           ,
        Key_Left            = MKT_KEY_LEFT            ,
        Key_Down            = MKT_KEY_DOWN            ,
        Key_Up              = MKT_KEY_UP              ,
        Key_Page_Up         = MKT_KEY_PAGE_UP         ,
        Key_Page_Down       = MKT_KEY_PAGE_DOWN       ,
        Key_Home            = MKT_KEY_HOME            ,
        Key_End             = MKT_KEY_END             ,
        Key_Caps_Lock       = MKT_KEY_CAPS_LOCK       ,
        Key_Scroll_Lock     = MKT_KEY_SCROLL_LOCK     ,
        Key_Num_Lock        = MKT_KEY_NUM_LOCK        ,
        Key_Print_Screen    = MKT_KEY_PRINT_SCREEN    ,
        Key_Pause           = MKT_KEY_PAUSE           ,

        Key_F1              = MKT_KEY_F1              ,
        Key_F2              = MKT_KEY_F2              ,
        Key_F3              = MKT_KEY_F3              ,
        Key_F4              = MKT_KEY_F4              ,
        Key_F5              = MKT_KEY_F5              ,
        Key_F6              = MKT_KEY_F6              ,
        Key_F7              = MKT_KEY_F7              ,
        Key_F8              = MKT_KEY_F8              ,
        Key_F9              = MKT_KEY_F9              ,
        Key_F10             = MKT_KEY_F10             ,
        Key_F11             = MKT_KEY_F11             ,
        Key_F12             = MKT_KEY_F12             ,
        Key_F13             = MKT_KEY_F13             ,
        Key_F14             = MKT_KEY_F14             ,
        Key_F15             = MKT_KEY_F15             ,
        Key_F16             = MKT_KEY_F16             ,
        Key_F17             = MKT_KEY_F17             ,
        Key_F18             = MKT_KEY_F18             ,
        Key_F19             = MKT_KEY_F19             ,
        Key_F20             = MKT_KEY_F20             ,
        Key_F21             = MKT_KEY_F21             ,
        Key_F22             = MKT_KEY_F22             ,
        Key_F23             = MKT_KEY_F23             ,
        Key_F24             = MKT_KEY_F24             ,
        Key_F25             = MKT_KEY_F25             ,

        Key_KP_0            = MKT_KEY_KP_0            ,
        Key_KP_1            = MKT_KEY_KP_1            ,
        Key_KP_2            = MKT_KEY_KP_2            ,
        Key_KP_3            = MKT_KEY_KP_3            ,
        Key_KP_4            = MKT_KEY_KP_4            ,
        Key_KP_5            = MKT_KEY_KP_5            ,
        Key_KP_6            = MKT_KEY_KP_6            ,
        Key_KP_7            = MKT_KEY_KP_7            ,
        Key_KP_8            = MKT_KEY_KP_8            ,
        Key_KP_9            = MKT_KEY_KP_9            ,
        Key_KP_Decimal      = MKT_KEY_KP_DECIMAL      ,
        Key_KP_Divide       = MKT_KEY_KP_DIVIDE       ,
        Key_KP_Multiply     = MKT_KEY_KP_MULTIPLY     ,
        Key_KP_Subtract     = MKT_KEY_KP_SUBTRACT     ,
        Key_KP_Add          = MKT_KEY_KP_ADD          ,
        Key_KP_Enter        = MKT_KEY_KP_ENTER        ,
        Key_KP_Equal        = MKT_KEY_KP_EQUAL        ,

        Key_Left_Shift      = MKT_KEY_LEFT_SHIFT      ,
        Key_Left_Control    = MKT_KEY_LEFT_CONTROL    ,
        Key_Left_Alt        = MKT_KEY_LEFT_ALT        ,
        Key_Left_Super      = MKT_KEY_LEFT_SUPER      ,
        Key_Right_Shift     = MKT_KEY_RIGHT_SHIFT     ,
        Key_Right_Control   = MKT_KEY_RIGHT_CONTROL   ,
        Key_Right_Alt       = MKT_KEY_RIGHT_ALT       ,
        Key_Right_Super     = MKT_KEY_RIGHT_SUPER     ,
        Key_Menu            = MKT_KEY_MENU            ,
    };

    MKT_NODISCARD inline auto GetStringRepresentation(KeyCode code) -> std::string_view {
        switch (code) {
            case Key_Space           : return KEY_STRING_REPRESENTATION(MKT_KEY_SPACE)         ;
            case Key_Apostrophe      : return KEY_STRING_REPRESENTATION(MKT_KEY_APOSTROPHE)    ;
            case Key_Comma           : return KEY_STRING_REPRESENTATION(MKT_KEY_COMMA)         ;
            case Key_Minus           : return KEY_STRING_REPRESENTATION(MKT_KEY_MINUS)         ;
            case Key_Period          : return KEY_STRING_REPRESENTATION(MKT_KEY_PERIOD)        ;
            case Key_Slash           : return KEY_STRING_REPRESENTATION(MKT_KEY_SLASH)         ;
            case Key_0               : return KEY_STRING_REPRESENTATION(MKT_KEY_0)             ;
            case Key_1               : return KEY_STRING_REPRESENTATION(MKT_KEY_1)             ;
            case Key_2               : return KEY_STRING_REPRESENTATION(MKT_KEY_2)             ;
            case Key_3               : return KEY_STRING_REPRESENTATION(MKT_KEY_3)             ;
            case Key_4               : return KEY_STRING_REPRESENTATION(MKT_KEY_4)             ;
            case Key_5               : return KEY_STRING_REPRESENTATION(MKT_KEY_5)             ;
            case Key_6               : return KEY_STRING_REPRESENTATION(MKT_KEY_6)             ;
            case Key_7               : return KEY_STRING_REPRESENTATION(MKT_KEY_7)             ;
            case Key_8               : return KEY_STRING_REPRESENTATION(MKT_KEY_8)             ;
            case Key_9               : return KEY_STRING_REPRESENTATION(MKT_KEY_9)             ;

            case Key_Semicolon       : return KEY_STRING_REPRESENTATION(MKT_KEY_SEMICOLON)     ;
            case Key_Equal           : return KEY_STRING_REPRESENTATION(MKT_KEY_EQUAL)         ;

            case Key_A               : return KEY_STRING_REPRESENTATION(MKT_KEY_A)             ;
            case Key_B               : return KEY_STRING_REPRESENTATION(MKT_KEY_B)             ;
            case Key_C               : return KEY_STRING_REPRESENTATION(MKT_KEY_C)             ;
            case Key_D               : return KEY_STRING_REPRESENTATION(MKT_KEY_D)             ;
            case Key_E               : return KEY_STRING_REPRESENTATION(MKT_KEY_E)             ;
            case Key_F               : return KEY_STRING_REPRESENTATION(MKT_KEY_F)             ;
            case Key_G               : return KEY_STRING_REPRESENTATION(MKT_KEY_G)             ;
            case Key_H               : return KEY_STRING_REPRESENTATION(MKT_KEY_H)             ;
            case Key_I               : return KEY_STRING_REPRESENTATION(MKT_KEY_I)             ;
            case Key_J               : return KEY_STRING_REPRESENTATION(MKT_KEY_J)             ;
            case Key_K               : return KEY_STRING_REPRESENTATION(MKT_KEY_K)             ;
            case Key_L               : return KEY_STRING_REPRESENTATION(MKT_KEY_L)             ;
            case Key_M               : return KEY_STRING_REPRESENTATION(MKT_KEY_M)             ;
            case Key_N               : return KEY_STRING_REPRESENTATION(MKT_KEY_N)             ;
            case Key_O               : return KEY_STRING_REPRESENTATION(MKT_KEY_O)             ;
            case Key_P               : return KEY_STRING_REPRESENTATION(MKT_KEY_P)             ;
            case Key_Q               : return KEY_STRING_REPRESENTATION(MKT_KEY_Q)             ;
            case Key_R               : return KEY_STRING_REPRESENTATION(MKT_KEY_R)             ;
            case Key_S               : return KEY_STRING_REPRESENTATION(MKT_KEY_S)             ;
            case Key_T               : return KEY_STRING_REPRESENTATION(MKT_KEY_T)             ;
            case Key_U               : return KEY_STRING_REPRESENTATION(MKT_KEY_U)             ;
            case Key_V               : return KEY_STRING_REPRESENTATION(MKT_KEY_V)             ;
            case Key_W               : return KEY_STRING_REPRESENTATION(MKT_KEY_W)             ;
            case Key_X               : return KEY_STRING_REPRESENTATION(MKT_KEY_X)             ;
            case Key_Y               : return KEY_STRING_REPRESENTATION(MKT_KEY_Y)             ;
            case Key_Z               : return KEY_STRING_REPRESENTATION(MKT_KEY_Z)             ;
            case Key_Left_Bracket    : return KEY_STRING_REPRESENTATION(MKT_KEY_LEFT_BRACKET)  ;
            case Key_Backslash       : return KEY_STRING_REPRESENTATION(MKT_KEY_BACKSLASH)     ;
            case Key_Right_Bracket   : return KEY_STRING_REPRESENTATION(MKT_KEY_RIGHT_BRACKET) ;
            case Key_Grave_Accent    : return KEY_STRING_REPRESENTATION(MKT_KEY_GRAVE_ACCENT)  ;
            case Key_World_1         : return KEY_STRING_REPRESENTATION(MKT_KEY_WORLD_1)       ;
            case Key_World_2         : return KEY_STRING_REPRESENTATION(MKT_KEY_WORLD_2)       ;

            case Key_Escape          : return KEY_STRING_REPRESENTATION(MKT_KEY_ESCAPE)        ;
            case Key_Enter           : return KEY_STRING_REPRESENTATION(MKT_KEY_ENTER)         ;
            case Key_Tab             : return KEY_STRING_REPRESENTATION(MKT_KEY_TAB)           ;
            case Key_Backspace       : return KEY_STRING_REPRESENTATION(MKT_KEY_BACKSPACE)     ;
            case Key_Insert          : return KEY_STRING_REPRESENTATION(MKT_KEY_INSERT)        ;
            case Key_Delete          : return KEY_STRING_REPRESENTATION(MKT_KEY_DELETE)        ;
            case Key_Right           : return KEY_STRING_REPRESENTATION(MKT_KEY_RIGHT)         ;
            case Key_Left            : return KEY_STRING_REPRESENTATION(MKT_KEY_LEFT)          ;
            case Key_Down            : return KEY_STRING_REPRESENTATION(MKT_KEY_DOWN)          ;
            case Key_Up              : return KEY_STRING_REPRESENTATION(MKT_KEY_UP)            ;
            case Key_Page_Up         : return KEY_STRING_REPRESENTATION(MKT_KEY_PAGE_UP)       ;
            case Key_Page_Down       : return KEY_STRING_REPRESENTATION(MKT_KEY_PAGE_DOWN)     ;
            case Key_Home            : return KEY_STRING_REPRESENTATION(MKT_KEY_HOME)          ;
            case Key_End             : return KEY_STRING_REPRESENTATION(MKT_KEY_END)           ;
            case Key_Caps_Lock       : return KEY_STRING_REPRESENTATION(MKT_KEY_CAPS_LOCK)     ;
            case Key_Scroll_Lock     : return KEY_STRING_REPRESENTATION(MKT_KEY_SCROLL_LOCK)   ;
            case Key_Num_Lock        : return KEY_STRING_REPRESENTATION(MKT_KEY_NUM_LOCK)      ;
            case Key_Print_Screen    : return KEY_STRING_REPRESENTATION(MKT_KEY_PRINT_SCREEN)  ;
            case Key_Pause           : return KEY_STRING_REPRESENTATION(MKT_KEY_PAUSE)         ;

            case Key_F1              : return KEY_STRING_REPRESENTATION(MKT_KEY_F1)            ;
            case Key_F2              : return KEY_STRING_REPRESENTATION(MKT_KEY_F2)            ;
            case Key_F3              : return KEY_STRING_REPRESENTATION(MKT_KEY_F3)            ;
            case Key_F4              : return KEY_STRING_REPRESENTATION(MKT_KEY_F4)            ;
            case Key_F5              : return KEY_STRING_REPRESENTATION(MKT_KEY_F5)            ;
            case Key_F6              : return KEY_STRING_REPRESENTATION(MKT_KEY_F6)            ;
            case Key_F7              : return KEY_STRING_REPRESENTATION(MKT_KEY_F7)            ;
            case Key_F8              : return KEY_STRING_REPRESENTATION(MKT_KEY_F8)            ;
            case Key_F9              : return KEY_STRING_REPRESENTATION(MKT_KEY_F9)            ;
            case Key_F10             : return KEY_STRING_REPRESENTATION(MKT_KEY_F10)           ;
            case Key_F11             : return KEY_STRING_REPRESENTATION(MKT_KEY_F11)           ;
            case Key_F12             : return KEY_STRING_REPRESENTATION(MKT_KEY_F12)           ;
            case Key_F13             : return KEY_STRING_REPRESENTATION(MKT_KEY_F13)           ;
            case Key_F14             : return KEY_STRING_REPRESENTATION(MKT_KEY_F14)           ;
            case Key_F15             : return KEY_STRING_REPRESENTATION(MKT_KEY_F15)           ;
            case Key_F16             : return KEY_STRING_REPRESENTATION(MKT_KEY_F16)           ;
            case Key_F17             : return KEY_STRING_REPRESENTATION(MKT_KEY_F17)           ;
            case Key_F18             : return KEY_STRING_REPRESENTATION(MKT_KEY_F18)           ;
            case Key_F19             : return KEY_STRING_REPRESENTATION(MKT_KEY_F19)           ;
            case Key_F20             : return KEY_STRING_REPRESENTATION(MKT_KEY_F20)           ;
            case Key_F21             : return KEY_STRING_REPRESENTATION(MKT_KEY_F21)           ;
            case Key_F22             : return KEY_STRING_REPRESENTATION(MKT_KEY_F22)           ;
            case Key_F23             : return KEY_STRING_REPRESENTATION(MKT_KEY_F23)           ;
            case Key_F24             : return KEY_STRING_REPRESENTATION(MKT_KEY_F24)           ;
            case Key_F25             : return KEY_STRING_REPRESENTATION(MKT_KEY_F25)           ;

            case Key_KP_0            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_0)          ;
            case Key_KP_1            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_1)          ;
            case Key_KP_2            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_2)          ;
            case Key_KP_3            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_3)          ;
            case Key_KP_4            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_4)          ;
            case Key_KP_5            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_5)          ;
            case Key_KP_6            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_6)          ;
            case Key_KP_7            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_7)          ;
            case Key_KP_8            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_8)          ;
            case Key_KP_9            : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_9)          ;
            case Key_KP_Decimal      : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_DECIMAL)    ;
            case Key_KP_Divide       : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_DIVIDE)     ;
            case Key_KP_Multiply     : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_MULTIPLY)   ;
            case Key_KP_Subtract     : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_SUBTRACT)   ;
            case Key_KP_Add          : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_ADD)        ;
            case Key_KP_Enter        : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_ENTER)      ;
            case Key_KP_Equal        : return KEY_STRING_REPRESENTATION(MKT_KEY_KP_EQUAL)      ;

            case Key_Left_Shift      : return KEY_STRING_REPRESENTATION(MKT_KEY_LEFT_SHIFT)    ;
            case Key_Left_Control    : return KEY_STRING_REPRESENTATION(MKT_KEY_LEFT_CONTROL)  ;
            case Key_Left_Alt        : return KEY_STRING_REPRESENTATION(MKT_KEY_LEFT_ALT)      ;
            case Key_Left_Super      : return KEY_STRING_REPRESENTATION(MKT_KEY_LEFT_SUPER)    ;
            case Key_Right_Shift     : return KEY_STRING_REPRESENTATION(MKT_KEY_RIGHT_SHIFT)   ;
            case Key_Right_Control   : return KEY_STRING_REPRESENTATION(MKT_KEY_RIGHT_CONTROL) ;
            case Key_Right_Alt       : return KEY_STRING_REPRESENTATION(MKT_KEY_RIGHT_ALT)     ;
            case Key_Right_Super     : return KEY_STRING_REPRESENTATION(MKT_KEY_RIGHT_SUPER)   ;
            case Key_Menu            : return KEY_STRING_REPRESENTATION(MKT_KEY_MENU)          ;
            default                  : return "Unknown key code";
        }
    }
}
#endif // MIKOTO_KEYCODES_HH
