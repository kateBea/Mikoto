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

#ifndef MIKOTO_PLATFORM_STEERING_WHEEL_CODES_HH
#define MIKOTO_PLATFORM_STEERING_WHEEL_CODES_HH

#include <Core/Types.hh>

namespace mikoto::platform {

    // Nonnegative values are native device indices. Wheel layouts vary by model.
    // https://wiki.libsdl.org/SDL3/SDL_GetJoystickAxis
    enum class SteeringWheelAxis : core::i16 {
        eUnknown = -1,
        eFirst = 0
    };

    // https://wiki.libsdl.org/SDL3/SDL_GetJoystickButton
    enum class SteeringWheelButton : core::i16 {
        eUnknown = -1,
        eFirst = 0
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_STEERING_WHEEL_CODES_HH
