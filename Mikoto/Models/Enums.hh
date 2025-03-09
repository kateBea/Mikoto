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

    enum class MapType {
        TEXTURE_2D_INVALID,
        TEXTURE_2D_DIFFUSE,
        TEXTURE_2D_SPECULAR,
        TEXTURE_2D_EMISSIVE,
        TEXTURE_2D_NORMAL,
        TEXTURE_2D_ROUGHNESS,
        TEXTURE_2D_METALLIC,
        TEXTURE_2D_AMBIENT_OCCLUSION,
        TEXTURE_2D_COUNT,

        TEXTURE_CUBE,
    };


    enum MaterialPass {
        MATERIAL_PASS_COLOR = 1,
        MATERIAL_PASS_PBR = 2,
        MATERIAL_PASS_WIREFRAME = 3,
        MATERIAL_PASS_COMPUTE = 4,
        MATERIAL_PASS_OUTLINE = 5,
    };

    enum class FileType {
        UNKNOWN_IMAGE_TYPE,

        PNG_IMAGE_TYPE,
        JPEG_IMAGE_TYPE,
        JPG_IMAGE_TYPE,
        BMP_IMAGE_TYPE,
        GIF_IMAGE_TYPE,
        TIFF_IMAGE_TYPE,
        WEBP_IMAGE_TYPE,
        ICO_IMAGE_TYPE,
        SVG_VECTOR_IMAGE_TYPE,

        PDF_DOCUMENT_TYPE,
        TEXT_DOCUMENT_TYPE,


        MP3_AUDIO_TYPE,
        WAV_AUDIO_TYPE,
        OGG_AUDIO_TYPE,
        FLAC_AUDIO_TYPE,
        AAC_AUDIO_TYPE,
        WMA_AUDIO_TYPE,
    };

    enum class MaterialType {
        PBR,
        STANDARD,
    };

    enum ProjectionType {
        ORTHOGRAPHIC = 0,
        PERSPECTIVE = 1,
    };

    /**
     * @brief Time units.
     * */
    enum class TimeUnit {
        SECONDS,      /**< Time unit in seconds. */
        MILLISECONDS, /**< Time unit in milliseconds. */
        MICROSECONDS, /**< Time unit in microseconds. */
        NANOSECONDS,  /**< Time unit in nanoseconds. */
    };

    enum BufferBit {
        COLOR_BUFFER_BIT = BIT_SET( 1 ),
        DEPTH_BUFFER_BIT = BIT_SET( 2 ),
    };


    enum class LightType {
        DIRECTIONAL_LIGHT_TYPE = 0,
        POINT_LIGHT_TYPE = 1,
        SPOT_LIGHT_TYPE = 2,
    };

    enum class GraphicsAPI {
        VULKAN_API,
        UNKNOWN,
    };

    /**
     * @brief Enum defining cursor input modes.
     * */
    enum CursorInputMode {
        CURSOR_NORMAL = 0,   /**< The default cursor mode. */
        CURSOR_HIDDEN = 1,   /**< The hidden cursor mode. */
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
        EMPTY_EVENT_CATEGORY = BIT_SET( 0 ),

        APPLICATION_EVENT_CATEGORY = BIT_SET( 1 ),
        INPUT_EVENT_CATEGORY = BIT_SET( 2 ),
        WINDOW_EVENT_CATEGORY = BIT_SET( 3 ),
        KEY_EVENT_CATEGORY = BIT_SET( 4 ),
        MOUSE_EVENT_CATEGORY = BIT_SET( 5 ),
        MOUSE_BUTTON_EVENT_CATEGORY = BIT_SET( 6 ),
        PANEL_EVENT_CATEGORY = BIT_SET( 8 ),

        EVENT_CATEGORY_COUNT = BIT_SET( 9 ),
    };

    enum ShaderStage {
        VERTEX_STAGE = BIT_SET( 1 ),
        FRAGMENT_STAGE = BIT_SET( 2 ),
        GEOMETRY_STAGE = BIT_SET( 3 ),
        TESSELATION_STAGE = BIT_SET( 4 ),
        COMPUTE_STAGE = BIT_SET( 5 ),
    };
}// namespace Mikoto

#endif//MIKOTO_EDITOR_ENUMS_H
