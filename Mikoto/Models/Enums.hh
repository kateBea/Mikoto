//
// Created by kate on 12/15/24.
//

#ifndef MIKOTO_EDITOR_ENUMS_H
#define MIKOTO_EDITOR_ENUMS_H

#include <Common/Common.hh>

namespace Mikoto {

    /**
     * @brief Represents the current state of a system.
     * Can be used to control whether it is running, stopped or idling.
     * */
    enum class Status {
        RUNNING,
        STOPPED,
        IDLE,
    };

    enum class ShaderDataType {
        NONE,
        FLOAT_TYPE,     // Represents a single float data type
        FLOAT2_TYPE,    // Represents a two float data type
        FLOAT3_TYPE,    // Represents a three float data type
        FLOAT4_TYPE,    // Represents a four float data type

        MAT3_TYPE,      // Represents 3x3 float matrix data type
        MAT4_TYPE,      // Represents 4x4 float matrix data type

        INT_TYPE,       // Represents a single int data type
        INT2_TYPE,      // Represents a two int data type
        INT3_TYPE,      // Represents a three int data type
        INT4_TYPE,      // Represents a four int data type
        BOOL_TYPE,      // Represents a single boolean data type
        COUNT,
    };

    enum class Type {
        MATERIAL_TYPE_PBR,
        MATERIAL_TYPE_STANDARD,
    };

    /**
     * @brief Time units.
     * */
    enum class TimeUnit {
        SECONDS,         /**< Time unit in seconds. */
        MILLISECONDS,    /**< Time unit in milliseconds. */
        MICROSECONDS,    /**< Time unit in microseconds. */
        NANOSECONDS,     /**< Time unit in nanoseconds. */
    };

    enum BufferBit {
        COLOR_BUFFER_BIT = BIT_SET(1),
        DEPTH_BUFFER_BIT = BIT_SET(2),
    };


    enum class LightType {
        DIRECTIONAL_LIGHT_TYPE  = 0,
        POINT_LIGHT_TYPE        = 1,
        SPOT_LIGHT_TYPE         = 2,
    };


    enum class PrefabSceneObject {
        NO_PREFAB_OBJECT,
        SPRITE_PREFAB_OBJECT,
        CUBE_PREFAB_OBJECT,
        CUSTOM_MODEL_PREFAB_OBJECT,
        CONE_PREFAB_OBJECT,
        CYLINDER_PREFAB_OBJECT,
        SPHERE_PREFAB_OBJECT,
        SPONZA_PREFAB_OBJECT,
        COUNT_PREFAB_OBJECT,
    };

    enum class GraphicsAPI {
        VULKAN_API,
    };

    /**
     * @brief Enum defining cursor input modes.
     * */
    enum CursorInputMode {
        CURSOR_NORMAL   = 0, /**< The default cursor mode. */
        CURSOR_HIDDEN   = 1, /**< The hidden cursor mode. */
        CURSOR_DISABLED = 2, /**< The disabled cursor mode. */
    };

    /**
     * Simply specifies the type of an Event
     * */
    enum class EventType {
        EMPTY_EVENT,

        // Window events.
        // Category [WINDOW_EVENT_CATEGORY]
        WINDOW_RESIZE_EVENT,
        WINDOW_CLOSE_EVENT,
        WINDOW_MOVED_EVENT,

        // Application events.
        // Category [APPLICATION_EVENT_CATEGORY]
        APP_RENDER_EVENT,
        APP_UPDATE_EVENT,
        APP_TICK_EVENT,
        APP_CLOSE_EVENT,

        // Key Events.
        // Category [KEYBOARD_EVENT_CATEGORY]
        KEY_PRESSED_EVENT,
        KEY_RELEASED_EVENT,
        KEY_CHAR_EVENT,

        // Mouse button events.
        // Category [MOUSE_EVENT_CATEGORY]
        MOUSE_BUTTON_PRESSED_EVENT,
        MOUSE_BUTTON_RELEASED_EVENT,

        // Mouse events.
        // Category [MOUSE_BUTTON_EVENT_CATEGORY]
        MOUSE_MOVED_EVENT,
        MOUSE_SCROLLED_EVENT,

        // Panels Events
        // Category [PANEL_EVENT_CATEGORY]
        CAMERA_ENABLE_ROTATION,

        EVENT_TYPE_COUNT,
    };

    /**
     * Specifies the group of an event. This is defined
     * if and event handler may need to filter certain events,
     * this is an easy way to group all of our events into one set.
     * Bear in mind an event can be of different categories at the same time,
     * for this purpose we may want to query the category flags later by using a
     * bit wise OR which is supported by integers, for simplicity sake, this structure
     * is not declared as an enum class
     * */
    enum EventCategory : UInt32_T {
        EMPTY_EVENT_CATEGORY            =  BIT_SET(0),

        APPLICATION_EVENT_CATEGORY      =  BIT_SET(1),
        INPUT_EVENT_CATEGORY            =  BIT_SET(2),
        WINDOW_EVENT_CATEGORY           =  BIT_SET(3),
        KEY_EVENT_CATEGORY              =  BIT_SET(4),
        MOUSE_EVENT_CATEGORY            =  BIT_SET(5),
        MOUSE_BUTTON_EVENT_CATEGORY     =  BIT_SET(6),
        PANEL_EVENT_CATEGORY            = BIT_SET(8),

        EVENT_CATEGORY_COUNT            =  BIT_SET(9),
    };

    enum ShaderStage {
        SHADER_VERTEX_STAGE         = BIT_SET(1),
        SHADER_FRAGMENT_STAGE       = BIT_SET(2),
        SHADER_GEOMETRY_STAGE       = BIT_SET(3),
        SHADER_TESSELATION_STAGE    = BIT_SET(4),
    };
}

#endif //MIKOTO_EDITOR_ENUMS_H
