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

#ifndef MIKOTO_PLATFORM_UTILITY_HH
#define MIKOTO_PLATFORM_UTILITY_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Filesystem/Path.hh>

#include <Platform/KeyCodes.hh>
#include <Platform/MouseCodes.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <guiddef.h>

#endif

#if MIKOTO_MODULE_RENDERER

#include <SDL3/SDL.h>

#include <volk.h>

#include <Platform/Window.hh>

#endif

namespace mikoto::platform {

    struct InstalledFontFamily {
        eastl::string mFamily{};
        filesystem::Path mPath{};
        core::u32 mFaceIndex{};
    };

    /**
     * @brief Lists local Windows font families with an outline face suitable for UI text.
     * @return Font family names, paths and face indices. Empty when enumeration is unavailable.
     */
    MKT_NODISCARD auto GetInstalledFontFamilies() -> eastl::vector<InstalledFontFamily>;

    /**
     * @brief Retrieves a keyboard key or mouse button display name.
     * @param code Mikoto key to name.
     * @returns Borrowed static display name.
     */
    MKT_NODISCARD auto GetStringRepresentation( KeyCode code ) -> eastl::string_view;

    /**
     * @brief Retrieves a keyboard key or mouse button display name.
     * @param button Mikoto mouse button to name.
     * @returns Borrowed static display name.
     */
    MKT_NODISCARD auto GetStringRepresentation( MouseButton button ) -> eastl::string_view;

    namespace windows {

#if defined(MIKOTO_PLATFORM_WINDOWS)
        /**
         * @brief Formats a Windows GUID as a string.
         * @param guid Windows GUID to format.
         * @returns Formatted GUID, or empty text on failure.
         */
        MKT_NODISCARD auto GuidToString(const GUID& guid) -> eastl::string;
#endif

    }// namespace windows

#if MIKOTO_MODULE_RENDERER
    inline constexpr char kOwnerProperty[]{ "mikoto.window.owner" };

    // Internal platform bridge. Client input and window code only need IWindow/InputSystem.
    /**
     * @brief Initializes the selected native window backend.
     */
    auto InitializeWindowBackend() -> void;

    /**
     * @brief Shuts down the native window backend.
     */
    auto ShutdownWindowBackend() -> void;

    /**
     * @brief Queries Vulkan instance extensions required by the window backend.
     * @returns Required instance-extension names.
     */
    MKT_NODISCARD auto GetWindowVulkanExtensions() -> eastl::vector<core::cstr>;

    /**
     * @brief Creates a Vulkan surface for a native window.
     * @param window Borrowed window to operate on.
     * @param instance Vulkan instance that will own the surface.
     * @param[out] surface Receives the created Vulkan surface.
     * @returns Result returned by Vulkan surface creation.
     */
    MKT_NODISCARD auto CreateWindowVulkanSurface( const IWindow& window, VkInstance instance, VkSurfaceKHR* surface ) -> VkResult;
#endif


#if MIKOTO_MODULE_RENDERER
    /**
     * @brief Converts a Mikoto physical key to an SDL scancode for keyboard polling.
     * Uses the US-layout key positions; text input is handled separately.
     * @param key Mikoto physical key to convert.
     * @return The SDL scancode, or SDL_SCANCODE_UNKNOWN when the key has no mapping.
     */
    auto ConvertToSDLScancode( KeyCode key ) -> SDL_Scancode;

    /**
     * @brief Converts an SDL physical scancode to a Mikoto key for input events.
     * @param key SDL physical scancode from a keyboard event.
     * @return The Mikoto key, or Key_Unknown when the scancode has no mapping.
     */
    auto FromSDLScancode( SDL_Scancode key ) -> KeyCode;

    /**
     * @brief Converts SDL modifier flags to a combination of ModKey flags.
     * Combines left/right variants and preserves Shift, Control, Alt, Super,
     * Caps Lock and Num Lock. Other SDL flags are ignored.
     * @param modifiers SDL modifier bitmask from an event or SDL_GetModState().
     * @return The combined Mikoto modifier flags, or ModKey::eNone if none map.
     */
    auto FromSDLModifiers( SDL_Keymod modifiers ) -> ModKey;

    /**
     * @brief Converts an SDL event's button ID (not a button mask) to MouseButton.
     * @param button SDL mouse button ID from the event's button field.
     * @return The Mikoto button, or Mouse_Button_Unknown for unsupported IDs.
     */
    auto FromSDLMouseButton( core::u8 button ) -> MouseButton;

    /**
     * @brief Converts a Mikoto mouse button to a mask for SDL_GetMouseState().
     * @param button Mikoto mouse button to convert.
     * @return A single button bit, or zero for an unsupported button.
     */
    auto ConvertToSDLMouseButton( MouseButton button ) -> core::u32;
#endif

}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_UTILITY_HH
