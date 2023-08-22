/**
 * MouseButtons.hh
 * Created by kate on 5/31/23.
 * */

#ifndef KATE_ENGINE_MOUSE_BUTTONS_HH
#define KATE_ENGINE_MOUSE_BUTTONS_HH

#include <GLFW/glfw3.h>

#include <Utility/Common.hh>

#if defined(USE_GLFW_MOUSE_CODES)
    #define KT_MOUSE_BUTTON_1       GLFW_MOUSE_BUTTON_1
    #define KT_MOUSE_BUTTON_2       GLFW_MOUSE_BUTTON_2
    #define KT_MOUSE_BUTTON_3       GLFW_MOUSE_BUTTON_3
    #define KT_MOUSE_BUTTON_4       GLFW_MOUSE_BUTTON_4
    #define KT_MOUSE_BUTTON_5       GLFW_MOUSE_BUTTON_5
    #define KT_MOUSE_BUTTON_6       GLFW_MOUSE_BUTTON_6
    #define KT_MOUSE_BUTTON_7       GLFW_MOUSE_BUTTON_7
    #define KT_MOUSE_BUTTON_8       GLFW_MOUSE_BUTTON_8
    #define KT_MOUSE_BUTTON_LAST    GLFW_MOUSE_BUTTON_LAST
    #define KT_MOUSE_BUTTON_LEFT    GLFW_MOUSE_BUTTON_LEFT
    #define KT_MOUSE_BUTTON_RIGHT   GLFW_MOUSE_BUTTON_RIGHT
    #define KT_MOUSE_BUTTON_MIDDLE  GLFW_MOUSE_BUTTON_MIDDLE
#endif

#define MOUSE_BUTTON_STRING_REPRESENTATION(name) #name

namespace Mikoto {
    enum MouseButton : Int8_T {
        Mouse_Button_1 = KT_MOUSE_BUTTON_1,
        Mouse_Button_2 = KT_MOUSE_BUTTON_2,
        Mouse_Button_3 = KT_MOUSE_BUTTON_3,
        Mouse_Button_4 = KT_MOUSE_BUTTON_4,
        Mouse_Button_5 = KT_MOUSE_BUTTON_5,
        Mouse_Button_6 = KT_MOUSE_BUTTON_6,
        Mouse_Button_7 = KT_MOUSE_BUTTON_7,
        Mouse_Button_8 = KT_MOUSE_BUTTON_8,

        Mouse_Button_Last = GLFW_MOUSE_BUTTON_LAST,
        Mouse_Button_Left = GLFW_MOUSE_BUTTON_LEFT,
        Mouse_Button_Right = GLFW_MOUSE_BUTTON_RIGHT,
        Mouse_Button_Middle = GLFW_MOUSE_BUTTON_MIDDLE,
    };

    // TODO: fix
    MKT_NODISCARD inline auto GetStringRepresentation(MouseButton button) -> std::string_view {
        switch (button) {
            case Mouse_Button_1:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_1);
            case Mouse_Button_2:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_2);
            case Mouse_Button_3:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_3);
            case Mouse_Button_4:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_4);
            case Mouse_Button_5:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_5);
            case Mouse_Button_6:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_6);
            case Mouse_Button_7:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_7);
            case Mouse_Button_8:     return  MOUSE_BUTTON_STRING_REPRESENTATION(KT_MOUSE_BUTTON_8);
            default:                 return "Unknown mouse button";
        }
    }

}

#endif//KATE_ENGINE_MOUSE_BUTTONS_HH
