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

#ifndef MIKOTO_PLATFORM_GAMEPAD_CODES_HH
#define MIKOTO_PLATFORM_GAMEPAD_CODES_HH

#include <Core/Types.hh>

namespace mikoto::platform {

    // Face buttons are named by position so they work across controller layouts.
    // https://wiki.libsdl.org/SDL3/SDL_GamepadButton
    enum class GamepadButton : core::i8 {
        eUnknown = -1,
        eSouth = 0,
        eEast = 1,
        eWest = 2,
        eNorth = 3,
        eBack = 4,
        eGuide = 5,
        eStart = 6,
        eLeftStick = 7,
        eRightStick = 8,
        eLeftShoulder = 9,
        eRightShoulder = 10,
        eDPadUp = 11,
        eDPadDown = 12,
        eDPadLeft = 13,
        eDPadRight = 14,
        eMisc1 = 15,
        eRightPaddle1 = 16,
        eLeftPaddle1 = 17,
        eRightPaddle2 = 18,
        eLeftPaddle2 = 19,
        eTouchpad = 20,
        eMisc2 = 21,
        eMisc3 = 22,
        eMisc4 = 23,
        eMisc5 = 24,
        eMisc6 = 25,
        eCount
    };

    // https://wiki.libsdl.org/SDL3/SDL_GamepadAxis
    enum class GamepadAxis : core::i8 {
        eUnknown = -1,
        eLeftX = 0,
        eLeftY = 1,
        eRightX = 2,
        eRightY = 3,
        eLeftTrigger = 4,
        eRightTrigger = 5,
        eCount
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_GAMEPAD_CODES_HH
