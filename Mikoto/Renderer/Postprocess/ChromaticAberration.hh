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

#ifndef MIKOTO_RENDERER_POSTPROCESS_CHROMATIC_ABERRATION_HH
#define MIKOTO_RENDERER_POSTPROCESS_CHROMATIC_ABERRATION_HH

#include <Core/Types.hh>

namespace mikoto::renderer {

    struct ChromaticAberrationSettings {
        static constexpr core::f32 kMaxIntensity{ 32.0f };
        static constexpr core::f32 kMaxDirection{ 180.0f };

        bool mEnabled{};
        core::f32 mIntensity{ 3.0f }; // Pixel step between adjacent color channels.
        core::f32 mDirection{ 45.0f }; // Degrees; diagonal matches the original shader.
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_POSTPROCESS_CHROMATIC_ABERRATION_HH
